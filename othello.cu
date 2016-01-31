#include <cstdio>
#include <cmath>

#define OCCUPIED(board, field) ((board) & (1L<<(field)))
#define ON_BOARD(field) (0 <= (field) && (field) < 64)
#define EVALUATE(p1, p2) ((builtin_popcount(p1))-(builtin_popcount(p2)))

extern "C" {

const int INF = 128;
const int BOARD_SIZE = 8;
const int WARP_SIZE = 32;
const int MAX_DEPTH = 1;
const int STACK_SIZE = MAX_DEPTH * BOARD_SIZE * BOARD_SIZE;

__device__ void print(long long int X) {
    printf(">>%lld\n", X);
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (X & (1L << (i * BOARD_SIZE + j))) printf("1");
            else printf("0");
        }
        printf("\n");
    }
}

__device__ int builtin_popcount(long long int x) {
	int ret = 0;
	for(int i = 0; i < 64; ++i) {
		if(x&(1L<<i)) ++ret;
	}
	return ret;
}

/* args: Boards[N][2], N-count of boards, PossibleMoves[N][64], Results[N], player_token-'O' or 'X' */
__global__ void generate_moves(long long *Boards, int N, long long *PossibleMoves, int *Results) {
   
	int X[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
 	int Y[8] = {0, -BOARD_SIZE, -BOARD_SIZE, -BOARD_SIZE, 0, BOARD_SIZE, BOARD_SIZE, BOARD_SIZE};

    // 28KB
    __shared__ long long int S[4][STACK_SIZE][2];
    __shared__ int Result[4][STACK_SIZE];
    __shared__ int Parent[4][STACK_SIZE];
    __shared__ int Depth[4][STACK_SIZE];
    __shared__ int Size[4];

    /* INDICES */
    int thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    int board_id = thread_id / WARP_SIZE;
    if ( board_id >= N ) return;
    int field_id = (int) threadIdx.x % WARP_SIZE;
    int idx_board = board_id % 4;

    /* COPY INPUT BOARD */
    if (field_id == 0) {
        S[idx_board][0][0] = Boards[board_id];
        S[idx_board][0][1] = Boards[N + board_id];
        Depth[idx_board][0] = MAX_DEPTH;
        Parent[idx_board][0] = -1;
        Result[idx_board][0] = -INF;
        Size[idx_board] = 1;
    }
    __syncthreads();

    /* DFS */
    while (Size[idx_board] != 0) {
        int end = Size[idx_board] - 1;
        long long player_pawns = S[idx_board][end][0];
        long long opponent_pawns = S[idx_board][end][1];
        int parent = Parent[idx_board][end];
        int depth = Depth[idx_board][end];
        bool pop_vertex = false;
		
        if (depth == 0 && field_id == 0) Result[idx_board][end] = EVALUATE(player_pawns, opponent_pawns);	
        __syncthreads();

        // terminal
        if (field_id == 0 && Result[idx_board][end] != -INF) {												
            if(parent != -1) Result[idx_board][parent] = max(Result[idx_board][parent], -Result[idx_board][end]);			
            Size[idx_board] -= 1;
            pop_vertex = true;																			
        }
        __syncthreads();

        // visit current node
        if (field_id == 0 && !pop_vertex) Result[idx_board][end] = EVALUATE(player_pawns, opponent_pawns);

        // thread #idInBoard processes fields idInBoard and idInBoard + 32
        if(!pop_vertex)
        for (int k = 0; k < 2; ++k) {
            int field = WARP_SIZE * k + field_id;

            // Move cannot be applied if the field is occupied
            if (!OCCUPIED(player_pawns, field) && !OCCUPIED(opponent_pawns, field)) {    //check whether field is free
                bool flag = false;
                long long tmp = 0;

                // Try all 8 directions
                for (int i = 0; i < 8; ++i) {
                    // Direction of the move
                    int shift = X[i] + Y[i];
                    int opponents_field = field + shift;
                    int j = 0;

                    // Continue as long as fields in the row are occupied by the opponent
                    while (ON_BOARD(opponents_field) && OCCUPIED(opponent_pawns, opponents_field)) {
                        j++;
                        opponents_field += shift;
                    }

                    // If move is possible, Reversi!
                    if (ON_BOARD(opponents_field) && OCCUPIED(player_pawns, opponents_field) && j > 1) {
                        // Reversi!
                        while (opponents_field != field) {
                            tmp |= 1L << opponents_field;    //all gained fields
                            opponents_field -= shift;
                        }
                        flag = true;
                    }
                }

                // Place the new pawn
                tmp |= 1L << field;
                // Avoid if
                tmp *= flag;
                PossibleMoves[board_id * 64 + field] = tmp;            //save gained fields
            }
        }
        __syncthreads();

        //zero-thread in board pushes possible moves onto stack
        if (field_id == 0 && !pop_vertex) {
            for (int i = 0; i < BOARD_SIZE; ++i) {
                for (int j = 0; j < BOARD_SIZE; ++j) {
                    long long move = PossibleMoves[board_id * 64 + i * BOARD_SIZE + j];
                    if (move) {
                        int top = Size[idx_board];
                        Size[idx_board] = top + 1;				
                        S[idx_board][top][1] = player_pawns | move;        //old fields + gained
                        S[idx_board][top][0] = opponent_pawns ^ move;      //old fields xor gained by opponent player
                        Parent[idx_board][top] = end;
                        Result[idx_board][top] = -INF;
                        Depth[idx_board][top] = depth - 1;
                    }
                }
            }
        }
        __syncthreads();
    }
    Results[board_id] = Result[idx_board][0];
}

}
