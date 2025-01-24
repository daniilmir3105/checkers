#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <windows.h>

const int BOARD_SIZE = 8;
const char EMPTY = '.', WHITE = 'W', BLACK = 'B', WHITE_KING = 'w', BLACK_KING = 'b';

std::mutex mtx;

struct Move {
    int startRow, startCol, endRow, endCol;
};

// Структура для хранения состояния доски
class Board {
public:
    char board[BOARD_SIZE][BOARD_SIZE];

    Board() {
        resetBoard();
    }

    void resetBoard() {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if ((i + j) % 2 == 1) {
                    if (i < 3) {
                        board[i][j] = WHITE;
                    } else if (i > 4) {
                        board[i][j] = BLACK;
                    } else {
                        board[i][j] = EMPTY;
                    }
                } else {
                    board[i][j] = EMPTY;
                }
            }
        }
    }

    void display() {
        std::cout << ".|ABCDEFGH\n";
        for (int i = 0; i < BOARD_SIZE; ++i) {
            std::cout << BOARD_SIZE - i << "|";
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board[i][j] == EMPTY) {
                    std::cout << ".";
                } else {
                    std::cout << board[i][j];
                }
            }
            std::cout << "\n";
        }
    }

    bool isMoveValid(Move move, char player) {
        if (move.startRow < 0 || move.startRow >= BOARD_SIZE || move.startCol < 0 || move.startCol >= BOARD_SIZE ||
            move.endRow < 0 || move.endRow >= BOARD_SIZE || move.endCol < 0 || move.endCol >= BOARD_SIZE)
            return false;

        if (board[move.startRow][move.startCol] != player && board[move.startRow][move.startCol] != tolower(player)) {
            return false;
        }

        char target = board[move.endRow][move.endCol];
        if (target != EMPTY) {
            return false;
        }

        int rowDiff = std::abs(move.endRow - move.startRow);
        int colDiff = std::abs(move.endCol - move.startCol);

        if (board[move.startRow][move.startCol] == WHITE_KING || board[move.startRow][move.startCol] == BLACK_KING) {
            if (rowDiff == colDiff) {
                return true;
            }
        }

        if (rowDiff == 1 && colDiff == 1) {
            if ((player == WHITE && move.endRow < move.startRow) || (player == BLACK && move.endRow > move.startRow)) {
                return false;
            }
            return true;
        }

        if (rowDiff == 2 && colDiff == 2) {
            int midRow = (move.startRow + move.endRow) / 2;
            int midCol = (move.startCol + move.endCol) / 2;
            char opponent = (player == WHITE) ? BLACK : WHITE;

            if (board[midRow][midCol] == opponent || board[midRow][midCol] == tolower(opponent)) {
                return true;
            }
        }

        return false;
    }

    void makeMove(Move move, char player) {
        board[move.endRow][move.endCol] = player;
        board[move.startRow][move.startCol] = EMPTY;

        if ((player == WHITE && move.endRow == 7) || (player == BLACK && move.endRow == 0)) {
            board[move.endRow][move.endCol] = (player == WHITE) ? WHITE_KING : BLACK_KING;
        }

        int midRow = (move.startRow + move.endRow) / 2;
        int midCol = (move.startCol + move.endCol) / 2;
        if (std::abs(move.endRow - move.startRow) == 2) {
            board[midRow][midCol] = EMPTY;
        }
    }

    bool hasPossibleMoves(char player) {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board[i][j] == player || board[i][j] == tolower(player)) {
                    for (int di = -2; di <= 2; ++di) {
                        for (int dj = -2; dj <= 2; ++dj) {
                            int ni = i + di, nj = j + dj;
                            if (isMoveValid({i, j, ni, nj}, player)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    bool hasCaptureMoves(char player) {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board[i][j] == player || board[i][j] == tolower(player)) {
                    for (int di = -2; di <= 2; di += 4) {
                        for (int dj = -2; dj <= 2; dj += 4) {
                            int ni = i + di, nj = j + dj;
                            if (isMoveValid({i, j, ni, nj}, player)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

};

// Функция для совершения нескольких рубок подряд
bool tryCapture(Board& board, char player, int startRow, int startCol) {
    bool moveMade = false;
    for (int di = -2; di <= 2; di += 4) {
        for (int dj = -2; dj <= 2; dj += 4) {
            int ni = startRow + di, nj = startCol + dj;
            if (board.isMoveValid({startRow, startCol, ni, nj}, player)) {
                board.makeMove({startRow, startCol, ni, nj}, player);
                moveMade = true;
                if (board.hasCaptureMoves(player)) {
                    moveMade = tryCapture(board, player, ni, nj);
                }
                break;
            }
        }
        if (moveMade) break;
    }
    return moveMade;
}

// Параллельная версия функции для выполнения поиска рубок
void tryCaptureParallel(Board& board, char player, int startRow, int startCol, bool& moveMade) {
    for (int di = -2; di <= 2; di += 4) {
        for (int dj = -2; dj <= 2; dj += 4) {
            int ni = startRow + di, nj = startCol + dj;
            if (board.isMoveValid({startRow, startCol, ni, nj}, player)) {
                mtx.lock();
                board.makeMove({startRow, startCol, ni, nj}, player);
                moveMade = true;
                mtx.unlock();
                if (board.hasCaptureMoves(player)) {
                    bool nextMoveMade = false;
                    tryCaptureParallel(board, player, ni, nj, nextMoveMade);
                    moveMade = moveMade || nextMoveMade;
                }
                break;
            }
        }
        if (moveMade) break;
    }
}

void playerMove(Board& board, char player) {
    std::string input;
    bool hasCapture = board.hasCaptureMoves(player);

    if (hasCapture) {
        std::cout << "Принудительная рубка! Вы должны сделать рубку.\n";
    }

    while (true) {
        std::cout << "Введите ход (например, A6 B5): ";
        std::getline(std::cin, input);

        int startCol = input[0] - 'A';
        int startRow = BOARD_SIZE - (input[1] - '0');
        int endCol = input[3] - 'A';
        int endRow = BOARD_SIZE - (input[4] - '0');

        Move move = {startRow, startCol, endRow, endCol};

        if (board.isMoveValid(move, player)) {
            if (hasCapture) {
                int midRow = (move.startRow + move.endRow) / 2;
                int midCol = (move.startCol + move.endCol) / 2;
                char opponent = (player == WHITE) ? BLACK : WHITE;

                if (std::abs(move.startRow - move.endRow) == 2 && board.board[midRow][midCol] == opponent) {
                    board.makeMove(move, player);
                    break;
                } else {
                    std::cout << "Вы должны сделать рубку! Попробуйте снова.\n";
                }
            } else {
                board.makeMove(move, player);
                break;
            }
        } else {
            std::cout << "Некорректный ход. Попробуйте снова.\n";
        }
    }
}

void aiMove(Board& board, char aiPlayer) {
    bool moveMade = false;

    if (board.hasCaptureMoves(aiPlayer)) {
        std::vector<std::thread> threads;
        for (int i = 0; i < BOARD_SIZE && !moveMade; ++i) {
            for (int j = 0; j < BOARD_SIZE && !moveMade; ++j) {
                if (board.board[i][j] == aiPlayer) {
                    threads.push_back(std::thread([&, i, j]() {
                        bool localMoveMade = false;
                        tryCaptureParallel(board, aiPlayer, i, j, localMoveMade);
                        moveMade = moveMade || localMoveMade;
                    }));
                }
            }
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    if (!moveMade) {
        for (int i = 0; i < BOARD_SIZE && !moveMade; ++i) {
            for (int j = 0; j < BOARD_SIZE && !moveMade; ++j) {
                if (board.board[i][j] == aiPlayer) {
                    for (int di = -2; di <= 2; ++di) {
                        for (int dj = -2; dj <= 2; ++dj) {
                            int ni = i + di, nj = j + dj;
                            if (board.isMoveValid({i, j, ni, nj}, aiPlayer)) {
                                board.makeMove({i, j, ni, nj}, aiPlayer);
                                moveMade = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    Board board;
    char playerColor, aiColor;

    std::cout << "Выберите цвет (W для белых, B для черных): ";
    std::cin >> playerColor;
    std::cin.ignore();

    if (playerColor == WHITE) {
        aiColor = BLACK;
    } else {
        aiColor = WHITE;
        aiMove(board, aiColor);
        board.display();
    }

    while (true) {
        board.display();

        if (!board.hasPossibleMoves(playerColor)) {
            std::cout << "У вас нет возможных ходов. Игра завершена.\n";
            break;
        }
        playerMove(board, playerColor);
        board.display();

        if (!board.hasPossibleMoves(aiColor)) {
            std::cout << "У AI нет возможных ходов. Игра завершена.\n";
            break;
        }
        aiMove(board, aiColor);
    }

    return 0;
}
