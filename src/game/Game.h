/** See \see{Game}. */
struct Game;

int Game(void);
void Game_(void);
void GameUpdate(const int dt_ms);
struct Ship *GameGetPlayer(void);
float GameGetFoo(void);
