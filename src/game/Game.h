struct Game;

int Game(const int load);
void Game_(void);
void GameUpdate(const int t_ms, const int dt_ms);
/*float GameGetDeltaTime(void);*/
struct Ship *GameGetPlayer(void);
