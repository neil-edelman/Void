struct Game;

struct Game *Game(void);
void Game_(struct Game **gameptr);
void GameUpdate(struct Game *game, const int ms);
struct Sprite *GameEnumSprites(struct Game *game);
struct Vec2f GameGetCamera(const struct Game *game);
struct Resources *GameGetResources(const struct Game *game);
