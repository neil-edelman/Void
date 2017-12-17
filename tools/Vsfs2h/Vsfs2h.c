/* Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 Converts a vertex shader and a fragment shader (without " and any other
 tricks) into a C header. It is very delicate and not production code. It
 relies on {struct Shader} included in Draw.c

 @version 2017-05-15 Made much more complex.
 @since 2014
 @author Neil */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* printf */
#include <string.h> /* strncpy, strrchr */
#include <assert.h> /* assert */

/* constants */
static const char *programme   = "Vsfs2h";
static const char *year        = "2014";
static const int versionMajor  = 2;
static const int versionMinor  = 0;

/* private */
static void source_process(const char *const fn, const int is_output);
static void usage(const char *argvz);

static struct Text {
	char attribs[16][64];
	size_t attribs_size;
	char uniforms[512][64];
	size_t uniforms_size;
} text;
const size_t text_attribs_capacity = sizeof((struct Text *)0)->attribs
	/ sizeof *((struct Text *)0)->attribs,
	text_attribs_max = sizeof *((struct Text *)0)->attribs
	/ sizeof **((struct Text *)0)->attribs,
	text_uniforms_capacity = sizeof((struct Text *)0)->uniforms
	/ sizeof *((struct Text *)0)->uniforms,
	text_uniforms_max = sizeof *((struct Text *)0)->uniforms
	/ sizeof **((struct Text *)0)->uniforms;

static void prototype(const char *const name) {
	printf("int auto_%s(", name);
	if(text.attribs_size) {
		size_t i;
		for(i = 0; i < text.attribs_size; i++) {
			const char *const attrib = text.attribs[i];
			printf("const GLuint %s%s", attrib,
				i != text.attribs_size - 1 ? ", " : "");
		}
	} else {
		printf("void");
	}
	printf(")");
}

/** private (entry point)
 @param which file is turned into *.h
 @bug do not include \", it will break
 @bug the file name should be < 1024 */
int main(int argc, char **argv) {
	char name[1024], *a, *base[2], *un;
	size_t i;

	/* check that the user specified file */
	if(argc != 3 || *argv[1] == '-' || *argv[2] == '-') {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* extract the user-specifed filename from the vertex shader; change into a
	 c varname */
	for(i = 0; i < 2; i++) {
		if((base[i] = strrchr(argv[i + 1], '/'))) {
			base[i]++;
		} else {
			base[i] = argv[i];
		}
	}
	strncpy(name, base[0], sizeof(name) / sizeof(char));
	name[sizeof(name) / sizeof(char) - 1] = '\0';
	for(a = name; *a; a++) {
		if((*a < 'A' || *a > 'Z')
			&& (*a < 'a' || *a > 'z')
			&& (*a < '0' || *a > '9' || a == name)) { *a = '\0'; break; }
	}

	/* read in all values of things */
	source_process(argv[1], 0);
	source_process(argv[2], 0);

	/* print output */
	printf("/** Auto-generated from %s and %s by %s %d.%d */\n\n",
		base[0], base[1], programme, versionMajor, versionMinor);
	printf("static struct Auto%sShader {\n"
		"\tconst char *vs_source;\n"
		"\tconst char *fs_source;\n"
		"\tGLuint compiled;\n", name);
	for(i = 0; i < text.uniforms_size; i++) {
		un = text.uniforms[i];
		printf("\tGLint %s;\n", un);
	}
	printf("} auto_%s_shader = {\n"
		"/* vs_source */", name);
	source_process(argv[1], 1), printf(",\n"
		"/* fs_source */");
	source_process(argv[2], 1), printf(",\n"
		"/* linked shader and uniform variables set at run-time */\n");
	for(i = 0; i <= text.uniforms_size; i++) {
		printf("0%s", i != text.uniforms_size ? ", " : "\n");
	}
	printf("};\n\n");
	prototype(name), printf(";\n\n"
	"/** Compiles a shader.\n"
	" @return Success. */\n");
	prototype(name), printf(" {\n"
	"\tGLuint vs = 0, fs = 0, shader = 0;\n"
	"\tGLint status;\n"
	"\tenum { S_NOERR, S_VERT, S_FRAG, S_LINK, S_VALIDATE } e = S_NOERR;\n\n"
	"\tdo {\n"
	"\t\tvs = glCreateShader(GL_VERTEX_SHADER);\n"
	"\t\tglShaderSource(vs, 1, &auto_%s_shader.vs_source, 0);\n"
	"\t\tglCompileShader(vs);\n"
	"\t\tglGetShaderiv(vs, GL_COMPILE_STATUS, &status);\n"
	"\t\tif(!status) { e = S_VERT; break; }\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: compiled vertex shader, Sdr%%u.\\n\", vs);\n"
	"", name, name);
	printf("\t\tfs = glCreateShader(GL_FRAGMENT_SHADER);\n"
	"\t\tglShaderSource(fs, 1, &auto_%s_shader.fs_source, 0);\n"
	"\t\tglCompileShader(fs);\n"
	"\t\tglGetShaderiv(fs, GL_COMPILE_STATUS, &status);\n"
	"\t\tif(!status) { e = S_FRAG; break; }\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: compiled fragment shader, Sdr%%u.\\n\", fs);\n"
	"\t\tshader = glCreateProgram();\n"
	"\t\tglAttachShader(shader, vs);\n"
	"\t\tglAttachShader(shader, fs);\n"
	"", name, name);
	for(i = 0; i < text.attribs_size; i++) {
		const char *const attrib = text.attribs[i];
		printf("\t\tglBindAttribLocation(shader, %s, \"%s\");\n",
			attrib, attrib);
	}
	printf("\t\tglLinkProgram(shader);\n"
	"\t\tglGetProgramiv(shader, GL_LINK_STATUS, &status);\n"
	"\t\tif(!status) { e = S_LINK; break; }\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: linked shader programme, Sdr%%u.\\n\", shader);\n"
	"\t\tglValidateProgram(shader);\n"
	"\t\tglGetProgramiv(shader, GL_VALIDATE_STATUS, &status);\n"
	"\t\tif(!status) { e = S_VALIDATE; break; }\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: validated shader, Sdr%%u.\\n\", shader);\n"
	"\t} while(0);\n"
	"\tif(e) {\n"
	"\t\tGLchar str[1024];\n"
	"\t\tconst char *const during[] = { \"none\", \"vertex compilation\",\n"
	"", name, name);
	printf("\t\t\t\"fragment compilation\", \"linking\", \"validation\" };\n"
	"\t\tglGetShaderInfoLog(vs, (GLsizei)sizeof(str), 0, str);\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: failed %%s;\\n%%s\", during[e], str);\n"
	"\t}\n"
	"\tif(fs) {\n"
	"\t\tglDeleteShader(fs);\n"
	"\t\tif(shader) glDetachShader(shader, fs);\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: erasing fragment shader, Sdr%%u.\\n\", fs);\n"
	"\t}\n"
	"\tif(vs) {\n"
	"\t\tglDeleteShader(vs);\n"
	"", name, name);
	printf("\t\tif(shader) glDetachShader(shader, vs);\n"
	"\t\tfprintf(stderr, \"auto_%s_shader_init: erasing vertex shader, Sdr%%u.\\n\", vs);\n"
	"\t}\n"
	"\tif(e) {\n"
	"\t\tif(shader) {\n"
	"\t\t\tglDeleteProgram(shader);\n"
	"\t\t\tfprintf(stderr, \"auto_%s_shader_init: erased shader, Sdr%%u.\\n\", shader);\n"
	"\t\t}\n"
	"\t\treturn 0;\n"
	"\t}\n"
	"\tglUseProgram(shader);\n"
	"\tauto_%s_shader.compiled = shader;\n"
	"", name, name, name);
	for(i = 0; i < text.uniforms_size; i++) {
		char *uniform = text.uniforms[i];
		printf("\tauto_%s_shader.%s = glGetUniformLocation(shader, \"%s\");\n",
			name, uniform, uniform);
	}
	printf("\tWindowIsGlError(\"%s\");\n\n"
	"\treturn 1;\n"
	"}\n\n"
	"void auto_%s_(void);\n\n"
	"void auto_%s_(void) {\n"
	"\tif(!auto_%s_shader.compiled) return;\n"
	"\tfprintf(stderr, \"~auto_%s_shader: erase shader, Sdr%%u.\\n\",\n"
	"\t\tauto_%s_shader.compiled);\n"
	"\tglDeleteProgram(auto_%s_shader.compiled);\n"
	"\nauto_%s_shader.compiled = 0;\n"
	"}\n", name, name, name, name, name, name, name, name);

	return EXIT_SUCCESS;
}

static void source_process(const char *const fn, const int is_output) {
	FILE *const fp = fopen(fn, "r");
	size_t len;
	char read[1024], *word, *var, *c;
	enum { ATTRIB, UNIFORM } type;

	if(!fp) { perror(fn); exit(EXIT_FAILURE); }
	while(fgets(read, sizeof(read) / sizeof(char), fp)) {
		if(!(len = strlen(read))) break;
		if(read[len - 1] == '\n') {
			read[len - 1] = '\0';
			if(is_output) { printf("\n\"%s\\n\"", read); continue; }
			/* it's very delicate! */
			if(!(word = strtok(read, " \t"))) continue;
			if(!strcmp("attribute", word)) {
				type = ATTRIB;
			} else if(!strcmp("uniform", word)) {
				type = UNIFORM;
			} else {
				continue;
			}
			if(!strtok(0, " \t")) continue; /* var type; we don't care */
			while((word = strtok(0, " ,;"))) {
				if(type == ATTRIB) {
					if(text.attribs_size >= text_attribs_capacity) fprintf
						(stderr, "Attribs capacity exceeded at %lu :0.\n",
						(unsigned long)text_attribs_capacity),
						exit(EXIT_FAILURE);
					var = text.attribs[text.attribs_size++];
					strncpy(var, word, text_attribs_max);
					var[text_attribs_max - 1] = '\0';
				} else {
					if(text.uniforms_size >= text_uniforms_capacity) fprintf
						(stderr, "Uniforms capacity exceeded at %lu :0.\n",
						(unsigned long)text_uniforms_capacity),
						exit(EXIT_FAILURE);
					var = text.uniforms[text.uniforms_size++];
					strncpy(var, word, text_uniforms_max);
					var[text_uniforms_max - 1] = '\0';
				}
				if((c = strchr(var, '['))) *c = '\0';
				/* fprintf(stderr, "%s: <%s>\n", type == ATTRIB ? "attrib" : "uniform", var); */
			}
		} else {
			if(is_output) printf("%s", read);
		}
	}
	fclose(fp);
}

static void usage(const char *argvz) {
	fprintf(stderr, "Usage: %s <vs> <fs>\n", argvz);
	fprintf(stderr, "Version %d.%d.\n\n", versionMajor, versionMinor);
	fprintf(stderr, "%s Copyright %s Neil Edelman\n", programme, year);
	fprintf(stderr, "This program comes with ABSOLUTELY NO WARRANTY.\n");
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see copying.txt.\n\n");
}
