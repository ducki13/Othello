#include <cstdio>
#include <vector>
#include <iostream>
#include <cmath>
#include "cuda.h"

using namespace std;

const int INF = 1e9 + 10;
struct Player;
struct Board;
struct Move;
struct State;
class Game;

struct Move {
	int x, y;
	
	Move() : x(-1), y(-1) {}
	Move(int x_, int y_) : x(x_), y(y_) {}
};

struct Player {
	/* token: unique player sign on the board, e.g. X or O.*/
	char token;
	Player() {}
	Player(char token_) : token(token_) {}
};

struct Board {
	/* This class hold info about current placement of pawns.
	 * X[], Y[] - set directions: W, W-S, S, E-S, E, E-N, N, W-N. */ //TODO: szajsen, kierunki swiata mi sie mylą, weźże porozkminiaj
	static const int SIZE = 8;
	static const int MOVES = 8;
	static const int X[8];
	static const int Y[8];
	//layout[0]-'O' pawns, layout[1]-'X' pawns
	long long int layout[2];
	
	void fire_bit(int x, int y, int idx) {
		layout[idx] |= 1L<<(x*SIZE+y);
	}
	
	bool fired(int x, int y, int idx) {
		return layout[idx] & 1L<<(x*SIZE+y);
	}
	
	void dissipate_bit(int x, int y, int idx) {
		if(fired(x, y, idx)) layout[idx] ^= 1L<<(x*SIZE+y);
	}
	
	char get_token(int x, int y) {
		if(fired(x, y, 0)) return 'O';
		if(fired(x, y, 1)) return 'X';
		return '-';
	}
	
	void set_token(int x, int y, char token) {
		if(token == 'O') {
			fire_bit(x, y, 0);
			dissipate_bit(x, y, 1);
		} else {
			fire_bit(x, y, 1);
			dissipate_bit(x, y, 0);
		}
	}
	
	bool equals(const Board & other) {
		return (layout[0] == other.layout[0]) && (layout[1] == other.layout[1]);
	}
	
	Board() {
		layout[0] = layout[1] = 0L;
		fire_bit(3, 3, 0);	//'O'
		fire_bit(4, 4, 0);	//'O'
		fire_bit(3, 4, 1);	//'X'
		fire_bit(4, 3, 1);	//'X'
	}
	
	void display() {
	/* Displays board. */
		long long int O = layout[0];
		long long int X = layout[1];
		
		printf("  ");
		for(int i = 0; i < SIZE; i++) printf("%d ", i);
		printf("\n");
		for(int i = 0; i < SIZE; ++i) {
            printf("%d ", i);
			for(int j = 0; j < SIZE; ++j) {
				if(O&(1L<<(i*SIZE+j))) printf("O ");
				else if(X&(1L<<(i*SIZE+j))) printf("X ");
				else printf("- ");
			}
			printf("\n");
		}
	}
	
	bool is_on_board(int x, int y) {
	/* Checks whether x and y is between the board constraints. */
		return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
	}
	
	bool is_on_board(const Move & move) {
	/* Checks whether move is valid. */
		return is_on_board(move.x, move.y);
	}
	
	bool move_is_possible(const Move & move, const Player & player) {
		/* If the square is empty, it determines if it is a valid
		 * move for the current player by looking for a string of
		 * opposite colored pieces adjacent to it that end in
		 * a piece belonging to the player. */
		if(!is_on_board(move) || (get_token(move.x, move.y) != '-')) return false;
		char opponent_token = (player.token == 'X') ? 'O' : 'X';
		for(int i = 0; i < MOVES; ++i) { 
			int x, y;
			int j = 1;
			while(is_on_board(x=(move.x + X[i]*j), y=(move.y + Y[i]*j)) && get_token(x, y) == opponent_token) ++j;
			if(is_on_board(x, y) && (get_token(x, y) == player.token) && (j > 1)) return true;//TODO: tutaj można by Score zwracać jakiegoś maksymalnego, czyli maxa po j-1
		}
		return false;
	}
	
 	vector<Move> generate_moves(Player player) {
		/* This function takes the current board and generates
		 * a list of possible moves for the current player:
		 * Steps through each square on the board. If the square
		 * is empty, it determines if it is a valid move for
		 * the current player. If the move is valid, it adds
		 * it to a list of valid moves. It returns this list.*/
		vector<Move> ret;
		for(int i = 0; i < SIZE; ++i) {
			for(int j = 0; j < SIZE; ++j) {
				Move move(i, j);
				if(move_is_possible(move, player)) {
					ret.push_back(move);
				}
			}
		}
		return ret;
	}
	
 	Board apply(const Move & move, const Player & player) {
		Board ret(*this);
		char opponent_token = (player.token == 'X') ? 'O' : 'X';

		for(int i = 0; i < MOVES; ++i) { 
			int x, y;
			int j = 1;
			while(is_on_board(x=(move.x + X[i]*j), y=(move.y + Y[i]*j)) && (get_token(x, y) == opponent_token)) ++j;
			if(is_on_board(x, y) && (get_token(x, y) == player.token)) {
				--j;
				while(j > 0) {
					ret.set_token(move.x + X[i]*j, move.y + Y[i]*j, player.token);
					--j;
				}
			}
		}
		ret.set_token(move.x, move.y, player.token);
		return ret;
	}
	
	int player_mobility(const Player & player) {
		/* Returns count of player's possible moves. */
		return generate_moves(player).size();
	}
};
const int Board::X[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
const int Board::Y[8] = {0, -1, -1, -1, 0, 1, 1, 1};

struct State {
	Board board;
	Player player;
	
	State() {}
	State(Board board_, Player player_) {
		board = board_;
		player = player_;
	}
	
	int evaluate_result() {
		int player_id = (player.token == 'O') ? 0 : 1;
		return __builtin_popcount(board.layout[player_id]);
	}
	
	int evaluate_opponent_result() {
		int opponent_id = (player.token == 'O') ? 1 : 0;
		return __builtin_popcount(board.layout[opponent_id]);
	}
	
	vector<Move> generate_moves() {
		return board.generate_moves(player);
	}
};

struct Cuda {
	const char *cuda_error_string(CUresult result) { 
		switch(result) {
			case CUDA_SUCCESS: return "No errors"; 
			case CUDA_ERROR_INVALID_VALUE: return "Invalid value"; 
			case CUDA_ERROR_OUT_OF_MEMORY: return "Out of memory"; 
			case CUDA_ERROR_NOT_INITIALIZED: return "Driver not initialized"; 
			case CUDA_ERROR_DEINITIALIZED: return "Driver deinitialized"; 

			case CUDA_ERROR_NO_DEVICE: return "No CUDA-capable device available"; 
			case CUDA_ERROR_INVALID_DEVICE: return "Invalid device"; 

			case CUDA_ERROR_INVALID_IMAGE: return "Invalid kernel image"; 
			case CUDA_ERROR_INVALID_CONTEXT: return "Invalid context"; 
			case CUDA_ERROR_CONTEXT_ALREADY_CURRENT: return "Context already current"; 
			case CUDA_ERROR_MAP_FAILED: return "Map failed"; 
			case CUDA_ERROR_UNMAP_FAILED: return "Unmap failed"; 
			case CUDA_ERROR_ARRAY_IS_MAPPED: return "Array is mapped"; 
			case CUDA_ERROR_ALREADY_MAPPED: return "Already mapped"; 
			case CUDA_ERROR_NO_BINARY_FOR_GPU: return "No binary for GPU"; 
			case CUDA_ERROR_ALREADY_ACQUIRED: return "Already acquired"; 
			case CUDA_ERROR_NOT_MAPPED: return "Not mapped"; 
			case CUDA_ERROR_NOT_MAPPED_AS_ARRAY: return "Mapped resource not available for access as an array"; 
			case CUDA_ERROR_NOT_MAPPED_AS_POINTER: return "Mapped resource not available for access as a pointer"; 
			case CUDA_ERROR_ECC_UNCORRECTABLE: return "Uncorrectable ECC error detected"; 
			case CUDA_ERROR_UNSUPPORTED_LIMIT: return "CUlimit not supported by device";    

			case CUDA_ERROR_INVALID_SOURCE: return "Invalid source"; 
			case CUDA_ERROR_FILE_NOT_FOUND: return "File not found"; 
			case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND: return "Link to a shared object failed to resolve"; 
			case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED: return "Shared object initialization failed"; 

			case CUDA_ERROR_INVALID_HANDLE: return "Invalid handle"; 

			case CUDA_ERROR_NOT_FOUND: return "Not found"; 

			case CUDA_ERROR_NOT_READY: return "CUDA not ready"; 

			case CUDA_ERROR_LAUNCH_FAILED: return "Launch failed"; 
			case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES: return "Launch exceeded resources"; 
			case CUDA_ERROR_LAUNCH_TIMEOUT: return "Launch exceeded timeout"; 
			case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING: return "Launch with incompatible texturing"; 

			case CUDA_ERROR_UNKNOWN: return "Unknown error"; 

			default: return "Unknown CUDA error value"; 
		} 
	}

	void check(CUresult res) {
		if(res != CUDA_SUCCESS) {
			cout << cuda_error_string(res) << endl;
			exit(1);
		}
	}
	
	CUdevice cuDevice;	//handler
	CUcontext cuContext;//context
	CUmodule cuModule;	//module
	
	Cuda() {
		cuInit(0);//init
		check(cuDeviceGet(&cuDevice, 0));//get handler		
		check(cuCtxCreate(&cuContext, 0, cuDevice));//create context
		cuModule = (CUmodule)0;//create module from binary file "othello.ptx"
		check(cuModuleLoad(&cuModule, "othello.ptx"));
	}
	
	void print(long long int X) {
		printf("%lld\n", X);
		for(int i = 0; i < 8; ++i) {
			for(int j = 0; j < 8; ++j) {
				if(X&(1L<<(i*8+j))) printf("1");
				else printf("0");
			}
			printf("\n");
		}
	}
	
	vector<int> generate_parallel_results(vector<State> & states) {
		CUfunction generate_moves_kernel;//get kernel handler from module
		check(cuModuleGetFunction(&generate_moves_kernel, cuModule, "generate_moves"));
		int size = states.size();
		int warp_size = 32;
		int threads_count = size*warp_size;
		char token = states[0].player.token;
		
		CUdeviceptr Boards_d, Results_d, PossibleMoves_d;
		
		check(cuMemAlloc(&Boards_d, size*2*sizeof(long long)));			//Boards[N][2]
		check(cuMemAlloc(&Results_d, size*sizeof(int)));				//Results[N]
		check(cuMemAlloc(&PossibleMoves_d, size*64*sizeof(long long)));	//PossibleMoves[N][64]

		long long* pawns_O = new long long[1];
		long long* pawns_X = new long long[1];
		for(int i = 0; i < states.size(); ++i) {
			pawns_O[0] = states[i].board.layout[0];
			pawns_X[0] = states[i].board.layout[1];
			check(cuMemcpyHtoD(Boards_d+i*sizeof(long long), (void*)pawns_O, sizeof(long long)));
			check(cuMemcpyHtoD(Boards_d+(size+i)*sizeof(long long), (void*)pawns_X, sizeof(long long)));
		}

		//assumed that max thread count is enough to handle all states in one kernel call
		int threads_x = 32*4;
		int threads_y = 1;
		int blocks_x = (size+3)/4;	//ceil
		int blocks_y = 1;

		void* args[] = {&Boards_d, &size, &PossibleMoves_d, &Results_d};
		check(cuLaunchKernel(generate_moves_kernel, blocks_x, blocks_y, 1,//dim in grid
								threads_x, threads_y, 1,//dim in block
								0, 0, args, 0));
		check(cuCtxSynchronize());

		int* Results = new int[size];
		check(cuMemcpyDtoH((void*)Results, Results_d, size*sizeof(int)));
		check(cuMemFree(Boards_d));
		check(cuMemFree(Results_d));
		check(cuMemFree(PossibleMoves_d));
		
		vector<int> R;
		for(int i = 0; i < size; ++i) R.push_back(Results[i]);
		delete[] Results;
		
		return R;
	}
	
	void clean_cuda() {
		check(cuCtxDestroy(cuContext));
	}
	
} cuda_instance;

class Game {
	/* This type contains all information specific to the current state
	 * of the game, including layout of the board and current player.
	 * players[2] : player[0] - user, player[1] - computer,
	 * idx: index of current player. */
	Board board;
	Player players[2];
	int idx;
	static const int MAX_DEPTH = 3;

public:
	static const int USER = 0;
	static const int COMPUTER = 1;
	
	Game() {}
	Game(const char & usr_token, const char & comp_token, const int & idx_) : idx(idx_) {
		players[0].token = usr_token;
		players[1].token = comp_token;
	}
	Game(const Board & board) : board(board) {}

	int current_player() {
		return idx;
	}

	void switch_player() {
		idx = (idx+1) & 1;
	}

	bool move_is_possible(const Move & move) {
		return board.move_is_possible(move, players[idx]);
	}

	void apply_move(const Move & move) {
		board = board.apply(move, players[idx]);
	}

	vector<State> states_gpu;
	
	void minmax_gpu(State state, int depth) {
		if(depth == MAX_DEPTH) {
			states_gpu.push_back(state);
			return;
		}
		vector<Move> moves = state.generate_moves();
		for(int i = 0; i < moves.size(); ++i) {
			Move move = moves[i];
			Board new_board = state.board.apply(move, state.player);
			char opponent_token = state.player.token == 'X' ? 'O' : 'X';
			minmax_gpu(State(new_board, Player(opponent_token)), depth+1);
		}
	}
	
	State minmax_cpu() {
	/* Returns state which gives max result from those which have beed returned from cuda. */
		vector<int> results = cuda_instance.generate_parallel_results(states_gpu);
		int maxx, idx;
		maxx = idx = -1;
		for(int i = 0; i < results.size(); ++i) {
			if(maxx < results[i]) {
				maxx = results[i];
				idx = i;
			}
		}
		return states_gpu[idx];
	}
	
	bool retrace(State current, State desired, int depth) {
	/* Checks whether current_state could generate desired. */
		if(depth == MAX_DEPTH) return false;
		if(current.board.equals(desired.board)) return true;
		vector<Move> moves = current.generate_moves();
		for(int i = 0; i < moves.size(); ++i) {
			Move move = moves[i];
			Board new_board = current.board.apply(move, current.player);
			char opponent_token = current.player.token == 'X' ? 'O' : 'X';
			if(retrace(State(new_board, Player(opponent_token)), desired, depth+1)) return true;
		}
		return false;
	}
	
	Move play(State start_state) {
		minmax_gpu(start_state, 0);
		
		//get desired state
		State max_state = minmax_cpu();
		//retrace path to it on the tree
		vector<Move> moves = start_state.generate_moves();
		char opponent_token = start_state.player.token == 'X' ? 'O' : 'X';
		for(int i = 0; i < moves.size(); ++i) {
			Board new_board = start_state.board.apply(moves[i], start_state.player);
			if(retrace(State(new_board, Player(opponent_token)), max_state, 0)) {
				states_gpu.clear();	
				return moves[i];
			}
		}
	}
	
	void play() {
		Move chosen = play(State(board, players[idx]));
		board = board.apply(chosen, players[idx]);
	}	
	
	bool is_finished() {
		/* Check whether neither of players has possible move. */
		return board.player_mobility(players[0]) == 0 && board.player_mobility(players[1]) == 0;
	}
	
	bool has_move(int pl_idx) {
		return board.player_mobility(players[pl_idx]) > 0;
	}
	
	void display_board() {
		board.display();
		printf("\n");
	}
	
	void display_result() {
		State current_state = State(board, players[idx]);
		int usr_sc = current_state.evaluate_result();
		int cmp_sc = current_state.evaluate_opponent_result();
		if(usr_sc > cmp_sc) printf("Congratulations! You're winner!\n");
		else if(usr_sc < cmp_sc) printf("Congratulations! You're loser!\n");
		else printf("Congratulations! You're tied!\n");
		printf("Result: You(%d) : Computer(%d)\n", usr_sc, cmp_sc);
		printf("Thank You!\n");
	}
};

int main() {
	printf("Hello!\n");
		
	/* Play. */
	Game game('X', 'O', 0);
	while(!game.is_finished()) {
		game.display_board();
		if((game.current_player() == Game::USER) && game.has_move(Game::USER)) {	//User turn
			int x, y;
			do {
				printf("Your turn! Type coordinates.\n");			
				scanf("%d%d", &y, &x);
			} while(!game.move_is_possible(Move(x, y)));
			game.apply_move(Move(x, y));
		} else if((game.current_player() == Game::COMPUTER) && game.has_move(Game::COMPUTER)) {	//Computer turn
			game.play();
		}
		game.switch_player();
	}
	
	game.display_board();
	game.display_result();
	
	return 0;
}
