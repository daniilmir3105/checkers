#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <windows.h>

// Константы игры
const int BOARD_SIZE = 8;              // Размер доски
const char EMPTY = '.',                // Пустая клетка
            WHITE = 'W',               // Белая шашка
            BLACK = 'B',              // Черная шашка
            WHITE_KING = 'w',         // Белая дамка
            BLACK_KING = 'b';         // Черная дамка

std::mutex mtx;  // Мьютекс для синхронизации потоков

// Структура для хранения координат хода
struct Move {
    int startRow, startCol;  // Начальная позиция
    int endRow, endCol;      // Конечная позиция
};

// Класс, представляющий игровую доску
class Board {
public:
    char board[BOARD_SIZE][BOARD_SIZE];  // Двумерный массив клеток

    Board() {
        resetBoard();  // Инициализация доски
    }

    // Сброс доски в начальное состояние
    void resetBoard() {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if ((i + j) % 2 == 1) {  // Только черные клетки
                    if (i < 3) {
                        board[i][j] = WHITE;  // Белые шашки вверху
                    } else if (i > 4) {
                        board[i][j] = BLACK;  // Черные шашки внизу
                    } else {
                        board[i][j] = EMPTY;
                    }
                } else {
                    board[i][j] = EMPTY;
                }
            }
        }
    }

    // Отображение доски в консоли
    void display() {
        std::cout << ".|ABCDEFGH\n";  // Заголовок столбцов
        for (int i = 0; i < BOARD_SIZE; ++i) {
            std::cout << BOARD_SIZE - i << "|";  // Номера строк
            for (int j = 0; j < BOARD_SIZE; ++j) {
                std::cout << board[i][j];
            }
            std::cout << "\n";
        }
    }

    // Проверка валидности хода
    bool isMoveValid(Move move, char player) {
        // Проверка границ доски
        if (move.startRow < 0 || move.startRow >= BOARD_SIZE ||
            move.startCol < 0 || move.startCol >= BOARD_SIZE ||
            move.endRow < 0 || move.endRow >= BOARD_SIZE ||
            move.endCol < 0 || move.endCol >= BOARD_SIZE)
            return false;

        // Проверка принадлежности шашки игроку
        char current = board[move.startRow][move.startCol];
        if (current != player && current != tolower(player))
            return false;

        // Конечная позиция должна быть пустой
        if (board[move.endRow][move.endCol] != EMPTY)
            return false;

        int rowDiff = abs(move.endRow - move.startRow);
        int colDiff = abs(move.endCol - move.startCol);

        // Проверка ходов для дамок
        if (current == WHITE_KING || current == BLACK_KING) {
            // Дамки могут ходить на любое расстояние по диагонали
            return rowDiff == colDiff;
        }

        // Обычные ходы (без взятия)
        if (rowDiff == 1 && colDiff == 1) {
            // Направление движения для обычных шашек
            if ((player == WHITE && move.endRow < move.startRow) ||
                (player == BLACK && move.endRow > move.startRow))
                return false;
            return true;
        }

        // Ход со взятием (на 2 клетки)
        if (rowDiff == 2 && colDiff == 2) {
            int midRow = (move.startRow + move.endRow) / 2;
            int midCol = (move.startCol + move.endCol) / 2;
            char opponent = (player == WHITE) ? BLACK : WHITE;

            // Проверка наличия вражеской шашки для взятия
            return (board[midRow][midCol] == opponent ||
                    board[midRow][midCol] == tolower(opponent));
        }

        return false;
    }

    // Выполнение хода на доске
    void makeMove(Move move, char player) {
        // Перемещение шашки
        board[move.endRow][move.endCol] = board[move.startRow][move.startCol];
        board[move.startRow][move.startCol] = EMPTY;

        // Превращение в дамку при достижении края
        if ((player == WHITE && move.endRow == BOARD_SIZE-1) ||
            (player == BLACK && move.endRow == 0)) {
            board[move.endRow][move.endCol] =
                (player == WHITE) ? WHITE_KING : BLACK_KING;
        }

        // Удаление срубленной шашки
        if (abs(move.endRow - move.startRow) == 2) {
            int midRow = (move.startRow + move.endRow) / 2;
            int midCol = (move.startCol + move.endCol) / 2;
            board[midRow][midCol] = EMPTY;
        }
    }

    // Проверка наличия возможных ходов у игрока
    bool hasPossibleMoves(char player) {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board[i][j] == player || board[i][j] == tolower(player)) {
                    // Проверка всех возможных направлений
                    for (int di = -2; di <= 2; ++di) {
                        for (int dj = -2; dj <= 2; ++dj) {
                            Move move = {i, j, i+di, j+dj};
                            if (isMoveValid(move, player)) return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    // Проверка наличия обязательных взятий
    bool hasCaptureMoves(char player) {
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board[i][j] == player || board[i][j] == tolower(player)) {
                    // Проверка только ходов со взятием (на 2 клетки)
                    for (int di = -2; di <= 2; di += 4) {
                        for (int dj = -2; dj <= 2; dj += 4) {
                            Move move = {i, j, i+di, j+dj};
                            if (isMoveValid(move, player)) return true;
                        }
                    }
                }
            }
        }
        return false;
    }
};

// Рекурсивная функция для выполнения серии взятий
bool tryCapture(Board& board, char player, int startRow, int startCol) {
    bool moveMade = false;
    // Проверка всех возможных направлений для взятия
    for (int di = -2; di <= 2; di += 4) {
        for (int dj = -2; dj <= 2; dj += 4) {
            int ni = startRow + di, nj = startCol + dj;
            Move move = {startRow, startCol, ni, nj};
            if (board.isMoveValid(move, player)) {
                board.makeMove(move, player);
                moveMade = true;
                // Рекурсивная проверка следующих взятий
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

// Параллельная версия функции для обработки взятий
void tryCaptureParallel(Board& board, char player, int startRow, int startCol, bool& moveMade) {
    for (int di = -2; di <= 2; di += 4) {
        for (int dj = -2; dj <= 2; dj += 4) {
            Move move = {startRow, startCol, startRow+di, startCol+dj};
            if (board.isMoveValid(move, player)) {
                std::lock_guard<std::mutex> lock(mtx);  // Захват мьютекса
                board.makeMove(move, player);
                moveMade = true;
                // Рекурсивная проверка следующих взятий
                if (board.hasCaptureMoves(player)) {
                    bool nextMove = false;
                    tryCaptureParallel(board, player, move.endRow, move.endCol, nextMove);
                    moveMade |= nextMove;
                }
                break;
            }
        }
        if (moveMade) break;
    }
}

// Обработка хода игрока
void playerMove(Board& board, char player) {
    std::string input;
    bool hasCapture = board.hasCaptureMoves(player);

    if (hasCapture) {
        std::cout << "Обязательное взятие! Вы должны побить шашку.\n";
    }

    while (true) {
        std::cout << "Введите ход (например, A3 B4): ";
        std::getline(std::cin, input);

        // Парсинг введенных данных
        int startCol = toupper(input[0]) - 'A';
        int startRow = BOARD_SIZE - (input[1] - '0');
        int endCol = toupper(input[3]) - 'A';
        int endRow = BOARD_SIZE - (input[4] - '0');

        Move move = {startRow, startCol, endRow, endCol};

        if (board.isMoveValid(move, player)) {
            if (hasCapture) {
                // Проверка выполнения обязательного взятия
                if (abs(move.startRow - move.endRow) == 2) {
                    board.makeMove(move, player);
                    break;
                } else {
                    std::cout << "Вы должны выполнить взятие!\n";
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

// Обработка хода компьютера
void aiMove(Board& board, char aiPlayer) {
    bool moveMade = false;

    // Сначала проверяем обязательные взятия
    if (board.hasCaptureMoves(aiPlayer)) {
        std::vector<std::thread> threads;
        // Параллельный поиск возможных взятий
        for (int i = 0; i < BOARD_SIZE && !moveMade; ++i) {
            for (int j = 0; j < BOARD_SIZE && !moveMade; ++j) {
                if (board.board[i][j] == aiPlayer ||
                    board.board[i][j] == tolower(aiPlayer)) {
                    threads.emplace_back([&, i, j]() {
                        bool localMove = false;
                        tryCaptureParallel(board, aiPlayer, i, j, localMove);
                        if (localMove) moveMade = true;
                    });
                }
            }
        }
        // Ожидание завершения всех потоков
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }

    // Если взятий нет - делаем обычный ход
    if (!moveMade) {
        for (int i = 0; i < BOARD_SIZE && !moveMade; ++i) {
            for (int j = 0; j < BOARD_SIZE && !moveMade; ++j) {
                if (board.board[i][j] == aiPlayer ||
                    board.board[i][j] == tolower(aiPlayer)) {
                    // Перебор всех возможных направлений
                    for (int di = -2; di <= 2; ++di) {
                        for (int dj = -2; dj <= 2; ++dj) {
                            Move move = {i, j, i+di, j+dj};
                            if (board.isMoveValid(move, aiPlayer)) {
                                board.makeMove(move, aiPlayer);
                                moveMade = true;
                                break;
                            }
                        }
                        if (moveMade) break;
                    }
                }
            }
        }
    }

    // Искусственная задержка для реалистичности
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
    SetConsoleOutputCP(CP_UTF8);  // Настройка кодировки консоли
    Board board;
    char playerColor, aiColor;

    // Выбор цвета игроком
    std::cout << "Выберите цвет (W - белые, B - черные): ";
    std::cin >> playerColor;
    std::cin.ignore();
    playerColor = toupper(playerColor);
    aiColor = (playerColor == WHITE) ? BLACK : WHITE;

    // Первый ход черных (если игрок выбрал белых)
    if (playerColor == WHITE) {
        board.display();
    } else {
        aiMove(board, aiColor);
        board.display();
    }

    // Основной игровой цикл
    while (true) {
        // Ход игрока
        if (!board.hasPossibleMoves(playerColor)) {
            std::cout << "Нет возможных ходов. Вы проиграли!\n";
            break;
        }
        playerMove(board, playerColor);
        board.display();

        // Ход компьютера
        if (!board.hasPossibleMoves(aiColor)) {
            std::cout << "У компьютера нет ходов. Вы победили!\n";
            break;
        }
        aiMove(board, aiColor);
        board.display();
    }

    return 0;
}

                    
