#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>
typedef struct{
    uint64_t whitePawns;
    uint64_t whiteKnights;
    uint64_t whiteBishops;
    uint64_t whiteRooks;
    uint64_t whiteQueens;
    uint64_t whiteKing;

    uint64_t blackPawns;
    uint64_t blackKnights;
    uint64_t blackBishops;
    uint64_t blackRooks;
    uint64_t blackQueens;
    uint64_t blackKing;

    int whiteTurn;
    int castlingRights;
    int enPassantSquare;
} Board;

Board board;

char startPosition[200];

char moveHistory[1024][6];
int moveCount = 0;

void clearBoard(Board *b){
    b->whitePawns=0;
    b->whiteKnights=0;
    b->whiteBishops=0;
    b->whiteRooks=0;
    b->whiteQueens=0;
    b->whiteKing=0;

    b->blackPawns=0;
    b->blackKnights=0;
    b->blackBishops=0;
    b->blackRooks=0;
    b->blackQueens=0;
    b->blackKing=0;
}

void initializeClassic(Board *b){
    clearBoard(b);
    // white
    b->whitePawns   = 0x000000000000FF00;
    b->whiteRooks   = 0x0000000000000081;
    b->whiteKnights = 0x0000000000000042;
    b->whiteBishops = 0x0000000000000024;
    b->whiteQueens  = 0x0000000000000008;
    b->whiteKing    = 0x0000000000000010;
    // black
    b->blackPawns   = 0x00FF000000000000;
    b->blackRooks   = 0x8100000000000000;
    b->blackKnights = 0x4200000000000000;
    b->blackBishops = 0x2400000000000000;
    b->blackQueens  = 0x0800000000000000;
    b->blackKing    = 0x1000000000000000;

    b->whiteTurn = 1;
    // KQkq
    b->castlingRights = 15;
    b->enPassantSquare = -1;

    strcpy(startPosition,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void createFEN(Board *b)
{
    char fen[200]="";
    int empty=0;

    for(int rank=7;rank>=0;rank--)
    {
        for(int file=0;file<8;file++)
        {
            int sq=rank*8+file;
            uint64_t mask=1ULL<<sq;

            char c=0;


            if(b->whitePawns&mask)c='P';
            else if(b->whiteKnights&mask)c='N';
            else if(b->whiteBishops&mask)c='B';
            else if(b->whiteRooks&mask)c='R';
            else if(b->whiteQueens&mask)c='Q';
            else if(b->whiteKing&mask)c='K';

            else if(b->blackPawns&mask)c='p';
            else if(b->blackKnights&mask)c='n';
            else if(b->blackBishops&mask)c='b';
            else if(b->blackRooks&mask)c='r';
            else if(b->blackQueens&mask)c='q';
            else if(b->blackKing&mask)c='k';



            if(c)
            {
                if(empty)
                {
                    char e[5];
                    sprintf(e,"%d",empty);
                    strcat(fen,e);
                    empty=0;
                }

                char s[2]={c,0};
                strcat(fen,s);
            }
            else
            {
                empty++;
            }
        }


        if(empty)
        {
            char e[5];
            sprintf(e,"%d",empty);
            strcat(fen,e);
            empty=0;
        }


        if(rank!=0)
            strcat(fen,"/");
    }


    strcat(fen, b->whiteTurn ? " w " : " b ");

    strcat(fen,"- - 0 1");


    strcpy(startPosition,fen);
}

int occupied(Board *b, int sq){
    uint64_t mask = 1ULL << sq;

    return
    (b->whitePawns   & mask) ||
    (b->whiteKnights & mask) ||
    (b->whiteBishops & mask) ||
    (b->whiteRooks   & mask) ||
    (b->whiteQueens  & mask) ||
    (b->whiteKing    & mask) ||

    (b->blackPawns   & mask) ||
    (b->blackKnights & mask) ||
    (b->blackBishops & mask) ||
    (b->blackRooks   & mask) ||
    (b->blackQueens  & mask) ||
    (b->blackKing    & mask);
}

int squareFromString(char file, char rank){
    int x = file - 'a';
    int y = rank - '1';

    if(x < 0 || x > 7 || y < 0 || y > 7)
        return -1;
    return y * 8 + x;
}

int column(int sq){
    return sq % 8;
}

int row(int sq){
    return sq / 8;
}

int clearDiagonal(int from, int to){
    int dx = column(to) - column(from);
    int dy = row(to) - row(from);

    int stepX = (dx > 0) ? 1 : -1;
    int stepY = (dy > 0) ? 1 : -1;


    int x = column(from) + stepX;
    int y = row(from) + stepY;


    while(x != column(to) && y != row(to))
    {
        int sq = y * 8 + x;

        if(occupied(&board, sq))
            return 0;

        x += stepX;
        y += stepY;
    }

    return 1;
}

int clearStraight(int from, int to){
    int dx = column(to) - column(from);
    int dy = row(to) - row(from);

    int stepX = 0;
    int stepY = 0;


    if(dx != 0)
        stepX = (dx > 0) ? 1 : -1;

    if(dy != 0)
        stepY = (dy > 0) ? 1 : -1;


    int x = column(from) + stepX;
    int y = row(from) + stepY;


    while(x != column(to) || y != row(to))
    {
        int sq = y * 8 + x;

        if(occupied(&board, sq))
            return 0;

        x += stepX;
        y += stepY;
    }


    return 1;
}

bool legitMove(char move[], bool whiteTurn){
    if(strlen(move) != 4)
        return false;
    //decoding coordinates
    int from = squareFromString(
        move[0],
        move[1]
    );
    int to = squareFromString(
        move[2],
        move[3]
    );
    //decoding coordinates

    if(from == -1 || to == -1)
        return false;
    uint64_t fromMask = 1ULL << from;
    uint64_t toMask   = 1ULL << to;
    /*
        PAWN
    */
    if(whiteTurn && (board.whitePawns & fromMask)){
        int dx = column(to) - column(from);
        int dy = row(to) - row(from);
        // обычный ход вверх
        if(dx == 0 && dy == 1)
        {
            if(!occupied(&board,to))
                return true;
        }
        // первый ход на две клетки
        if(row(from)==1 && dx==0 && dy==2)
        {
            int middle = from + 8;
            if(!occupied(&board,middle) && !occupied(&board,to))
            {
                return true;
            }
        }
        // взятие
        if(abs(dx)==1 && dy==1)
        {
            if(occupied(&board,to))
                return true; 
        }
        return false;
    }
    if(!whiteTurn && (board.blackPawns & fromMask)){
        int dx = column(to) - column(from);
        int dy = row(to) - row(from);
        // вниз
        if(dx==0 && dy==-1){
            if(!occupied(&board,to))
                return true;
        }

        // первый ход
        if(row(from)==6 && dx==0 && dy==-2){
            int middle = from - 8;

            if(!occupied(&board,middle) && !occupied(&board,to))
            {
                return true;
            }
        }
        // взятие
        if(abs(dx)==1 && dy==-1){
            if(occupied(&board,to))
                return true;
        }

        return false;
    }
    /*
        KNIGHT
    */
    if(whiteTurn && (board.whiteKnights & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        if((dx == 1 && dy == 2) || (dx == 2 && dy == 1)){
            // нельзя вставать на свою фигуру
            uint64_t whitePieces =
                board.whitePawns |
                board.whiteKnights |
                board.whiteBishops |
                board.whiteRooks |
                board.whiteQueens |
                board.whiteKing;
            if(!(whitePieces & toMask))
                return true;
        }
        return false;
    }
    if(!whiteTurn && (board.blackKnights & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        if((dx == 1 && dy == 2) || (dx == 2 && dy == 1))
        {
            // нельзя вставать на свою фигуру
            uint64_t blackPieces =
                board.blackPawns |
                board.blackKnights |
                board.blackBishops |
                board.blackRooks |
                board.blackQueens |
                board.blackKing;
            if(!(blackPieces & toMask))
                return true;
        }

        return false;
    }
    /*
        BISHOP
    */
    if(whiteTurn && (board.whiteBishops & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        // слон ходит только по диагонали
        if(dx == dy && dx != 0)
        {
            if(clearDiagonal(from,to))
            {
                uint64_t whitePieces =
                    board.whitePawns |
                    board.whiteKnights |
                    board.whiteBishops |
                    board.whiteRooks |
                    board.whiteQueens |
                    board.whiteKing;

                if(!(whitePieces & toMask))
                    return true;
            }
        }
        return false;
    }

    if(!whiteTurn && (board.blackBishops & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        if(dx == dy && dx != 0){
            if(clearDiagonal(from,to)){
                uint64_t blackPieces =
                    board.blackPawns |
                    board.blackKnights |
                    board.blackBishops |
                    board.blackRooks |
                    board.blackQueens |
                    board.blackKing;

                if(!(blackPieces & toMask))
                    return true;
            }
        }

        return false;
    }
    /*
        ROOK
    */
    if(whiteTurn && (board.whiteRooks & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        // движение по вертикали или горизонтали
        if(dx == 0 || dy == 0)
        {
            // чтобы не разрешить стояние на месте
            if(dx == 0 && dy == 0)
                return false;

            if(clearStraight(from,to))
            {
                uint64_t whitePieces =
                    board.whitePawns |
                    board.whiteKnights |
                    board.whiteBishops |
                    board.whiteRooks |
                    board.whiteQueens |
                    board.whiteKing;

                if(!(whitePieces & toMask))
                    return true;
            }
        }

        return false;
    }

    if(!whiteTurn && (board.blackRooks & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        if(dx == 0 || dy == 0){
            if(dx == 0 && dy == 0)
                return false;

            if(clearStraight(from,to)){
                uint64_t blackPieces =
                    board.blackPawns |
                    board.blackKnights |
                    board.blackBishops |
                    board.blackRooks |
                    board.blackQueens |
                    board.blackKing;

                if(!(blackPieces & toMask))
                    return true;
            }
        }
        return false;
    }
    /*
        QUEEN
    */
    if(whiteTurn && (board.whiteQueens & fromMask)){
    int dx = abs(column(to) - column(from));
    int dy = abs(row(to) - row(from));


    // диагональ или прямая
    if((dx == dy && dx != 0) ||
       (dx == 0 || dy == 0))
    {

        if(dx == 0 && dy == 0)
            return false;


        int clear = 0;


        if(dx == dy)
            clear = clearDiagonal(from,to);
        else
            clear = clearStraight(from,to);


        if(clear)
        {
            uint64_t whitePieces =
                board.whitePawns |
                board.whiteKnights |
                board.whiteBishops |
                board.whiteRooks |
                board.whiteQueens |
                board.whiteKing;


            if(!(whitePieces & toMask))
                return true;
        }
    }

    return false;
    }

    if(!whiteTurn && (board.blackQueens & fromMask)){
    int dx = abs(column(to) - column(from));
    int dy = abs(row(to) - row(from));


    if((dx == dy && dx != 0) ||
       (dx == 0 || dy == 0))
    {

        if(dx == 0 && dy == 0)
            return false;


        int clear;


        if(dx == dy)
            clear = clearDiagonal(from,to);
        else
            clear = clearStraight(from,to);


        if(clear)
        {
            uint64_t blackPieces =
                board.blackPawns |
                board.blackKnights |
                board.blackBishops |
                board.blackRooks |
                board.blackQueens |
                board.blackKing;


            if(!(blackPieces & toMask))
                return true;
        }
    }

    return false;
    }
    /*
    KING
    */
    if(whiteTurn && (board.whiteKing & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));


        if(dx <= 1 && dy <= 1){
            if(dx == 0 && dy == 0)
                return false;

            uint64_t whitePieces =
                board.whitePawns |
                board.whiteKnights |
                board.whiteBishops |
                board.whiteRooks |
                board.whiteQueens |
                board.whiteKing;

            if(!(whitePieces & toMask))
                return true;
        }

        return false;
    }

    if(!whiteTurn && (board.blackKing & fromMask)){
        int dx = abs(column(to) - column(from));
        int dy = abs(row(to) - row(from));

        if(dx <= 1 && dy <= 1){
            if(dx == 0 && dy == 0)
                return false;

            uint64_t blackPieces =
                board.blackPawns |
                board.blackKnights |
                board.blackBishops |
                board.blackRooks |
                board.blackQueens |
                board.blackKing;

            if(!(blackPieces & toMask))
                return true;
        }

        return false;
    }

    return false;
}

void capturePiece(int sq){
    uint64_t mask = 1ULL << sq;

    board.whitePawns   &= ~mask;
    board.whiteKnights &= ~mask;
    board.whiteBishops &= ~mask;
    board.whiteRooks   &= ~mask;
    board.whiteQueens  &= ~mask;
    board.whiteKing    &= ~mask;

    board.blackPawns   &= ~mask;
    board.blackKnights &= ~mask;
    board.blackBishops &= ~mask;
    board.blackRooks   &= ~mask;
    board.blackQueens  &= ~mask;
    board.blackKing    &= ~mask;
}

void changeBoard(char move[]){
    int from = squareFromString(move[0], move[1]);
    int to   = squareFromString(move[2], move[3]);

    uint64_t fromMask = 1ULL << from;
    uint64_t toMask   = 1ULL << to;

    if(occupied(&board, to))
    {
        capturePiece(to);
    }

    // белые пешки
    if(board.whitePawns & fromMask)
    {
        board.whitePawns &= ~fromMask;
        board.whitePawns |= toMask;
    }

    // белые кони
    else if(board.whiteKnights & fromMask)
    {
        board.whiteKnights &= ~fromMask;
        board.whiteKnights |= toMask;
    }

    // белые слоны
    else if(board.whiteBishops & fromMask)
    {
        board.whiteBishops &= ~fromMask;
        board.whiteBishops |= toMask;
    }

    // белые ладьи
    else if(board.whiteRooks & fromMask)
    {
        board.whiteRooks &= ~fromMask;
        board.whiteRooks |= toMask;
    }

    // белый ферзь
    else if(board.whiteQueens & fromMask)
    {
        board.whiteQueens &= ~fromMask;
        board.whiteQueens |= toMask;
    }

    // белый король
    else if(board.whiteKing & fromMask)
    {
        board.whiteKing &= ~fromMask;
        board.whiteKing |= toMask;
    }

    // черные пешки
    else if(board.blackPawns & fromMask)
    {
        board.blackPawns &= ~fromMask;
        board.blackPawns |= toMask;
    }

    else if(board.blackKnights & fromMask)
    {
        board.blackKnights &= ~fromMask;
        board.blackKnights |= toMask;
    }

    else if(board.blackBishops & fromMask)
    {
        board.blackBishops &= ~fromMask;
        board.blackBishops |= toMask;
    }

    else if(board.blackRooks & fromMask)
    {
        board.blackRooks &= ~fromMask;
        board.blackRooks |= toMask;
    }

    else if(board.blackQueens & fromMask)
    {
        board.blackQueens &= ~fromMask;
        board.blackQueens |= toMask;
    }

    else if(board.blackKing & fromMask)
    {
        board.blackKing &= ~fromMask;
        board.blackKing |= toMask;
    }

    // смена хода
    board.whiteTurn = !board.whiteTurn;
}

void initialize960(Board *b)
{
    clearBoard(b);

    srand(time(NULL));

    uint64_t back=0;

    int bishop1 = (rand()%4)*2;
    int bishop2 = (rand()%4)*2+1;

    back |= 1ULL<<bishop1;
    back |= 1ULL<<bishop2;

    int queen;

    do
    {
        queen=rand()%8;

    }while(back&(1ULL<<queen));

    back |= 1ULL<<queen;

    int knights=0;

    while(knights<2)
    {
        int sq=rand()%8;

        if(!(back&(1ULL<<sq)))
        {
            back |= 1ULL<<sq;
            knights++;
        }
    }

    int rook1=-1;
    int king=-1;
    int rook2=-1;

    for(int i=0;i<8;i++)
    {
        if(!(back&(1ULL<<i)))
        {
            if(rook1==-1)
                rook1=i;

            else if(king==-1)
                king=i;

            else
                rook2=i;
        }
    }
    b->whiteBishops = (1ULL<<bishop1) | (1ULL<<bishop2);
    b->whiteQueens =(1ULL<<queen);
    // оставшиеся пустые были кони
    b->whiteKnights=0;

    for(int i=0;i<8;i++)
    {
        if((back&(1ULL<<i)) &&
           i!=bishop1 &&
           i!=bishop2 &&
           i!=queen)
        {
            b->whiteKnights |= 1ULL<<i;
        }
    }

    b->whiteRooks =
        (1ULL<<rook1) |
        (1ULL<<rook2);

    b->whiteKing =
        (1ULL<<king);
    // pawns
    b->whitePawns =
        0x000000000000FF00;

    // black mirror


    b->blackPawns =
        b->whitePawns<<40;

    b->blackRooks =
        b->whiteRooks<<56;

    b->blackKing =
        b->whiteKing<<56;

    b->blackQueens =
        b->whiteQueens<<56;

    b->blackBishops =
        b->whiteBishops<<56;

    b->blackKnights =
        b->whiteKnights<<56;

    b->whiteTurn=1;
    b->castlingRights=15;
    b->enPassantSquare=-1;
}

void printBoard(Board *b)
{
    printf("    a b c d e f g h\n    ---------------\n");
    for(int rank=7;rank>=0;rank--)
    {
        printf("%d | ",rank+1);

        for(int file=0;file<8;file++)
        {
            int sq = rank*8+file;
            uint64_t m=1ULL<<sq;
            char c='.';
            if(b->whitePawns&m)c='P';
            if(b->whiteKnights&m)c='N';
            if(b->whiteBishops&m)c='B';
            if(b->whiteRooks&m)c='R';
            if(b->whiteQueens&m)c='Q';
            if(b->whiteKing&m)c='K';

            if(b->blackPawns&m)c='p';
            if(b->blackKnights&m)c='n';
            if(b->blackBishops&m)c='b';
            if(b->blackRooks&m)c='r';
            if(b->blackQueens&m)c='q';
            if(b->blackKing&m)c='k';

            printf("%c ",c);
        }
        printf(" | %d \n",rank+1);
    }
    printf("    ---------------\n    a b c d e f g h\n");
}

bool isGameOver(Board *b)
{
    if(b->whiteKing == 0)
    {
        printf("Black wins!\n");
        return true;
    }
    if(b->blackKing == 0)
    {
        printf("White wins!\n");
        return true;
    }

    // позже добавишь:
    // if(!hasLegalMoves(b))
    //     return true;

    return false;
}

void addMove(char move[])
{
    strcpy(moveHistory[moveCount], move);
    moveCount++;
}

void sendPosition(FILE *engineOut)
{
    fprintf(engineOut,"setoption name UCI_Chess960 value true\n");

    fprintf(engineOut,
    "position fen %s",
    startPosition);


    if(moveCount>0)
    {
        fprintf(engineOut," moves ");

        for(int i=0;i<moveCount;i++)
        {
            fprintf(engineOut,"%s ",
            moveHistory[i]);
        }
    }


    fprintf(engineOut,"\n");
}

void pvp(){
    char move[6];

    while(!isGameOver(&board))
    {
        printBoard(&board);

        if(board.whiteTurn)
            printf("\nWhite turn.\nMake your move: ");
        else
            printf("\nBlack turn.\nMake your move: ");


        scanf("%5s", move);


        if(legitMove(move, board.whiteTurn))
        {
            changeBoard(move);
        }
        else
        {
            printf("Illegal move\n");
        }
    }
}

void pvb(int color){
    printf("Bot mode\n");

    int to_engine[2];
    int from_engine[2];

    pipe(to_engine);
    pipe(from_engine);

    pid_t pid = fork();

    if(pid == 0)
    {
        dup2(to_engine[0], STDIN_FILENO);
        dup2(from_engine[1], STDOUT_FILENO);

        close(to_engine[1]);
        close(from_engine[0]);

        execl(
            "/home/beifang/code/chess/engines/Stockfish/src/stockfish",
            "stockfish",
            NULL
        );
        perror("stockfish");
        exit(1);
    }

    close(to_engine[0]);
    close(from_engine[1]);

    FILE *engineIn =
        fdopen(from_engine[0],"r");

    FILE *engineOut =
        fdopen(to_engine[1],"w");

    char line[1024];

    // UCI
    fprintf(engineOut,"uci\n");
    fflush(engineOut);

    while(fgets(line,sizeof(line),engineIn))
    {
        if(strncmp(line,"uciok",5)==0)
            break;
    }
    fprintf(engineOut,"isready\n");
    fflush(engineOut);

    while(fgets(line,sizeof(line),engineIn))
    {
        if(strncmp(line,"readyok",7)==0)
            break;
    }

    bool playerWhite;

    if(color == 1)
        playerWhite = true;
    else
        playerWhite = false;
    char move[6];

    while(!isGameOver(&board))
    {
        printBoard(&board);
        // ход игрока
        if(board.whiteTurn == playerWhite)
        {
            printf("\nYour move: ");
            scanf("%5s",move);
            if(legitMove(move, playerWhite))
            {
                changeBoard(move);
                addMove(move);
            }
            else
            {
                printf("Illegal move\n");
            }
        }
        // ход движка
        else
        {
            printf("\nStockfish thinking...\n");
            sendPosition(engineOut);

            fprintf(engineOut,
            "go depth 12\n");

            fflush(engineOut);

            while(fgets(line,sizeof(line),engineIn))
            {
                if(strncmp(line,"bestmove",8)==0)
                {

                    sscanf(line,"bestmove %5s",move);

                    printf("Stockfish plays: %s\n",move);

                    if(legitMove(move, board.whiteTurn)){
                        changeBoard(move);
                        addMove(move);
                    }
                    else{
                        printf("Chess engine prodused wrong move");
                    }
                    break;
                }
            }
        }
    }

    fprintf(engineOut,"quit\n");
    fflush(engineOut);

    fclose(engineIn);
    fclose(engineOut);
}

int main(){
    int gamemode;
    int startpos;
    int color;

    printf("Chess game\n");

    while(1)
    {
        printf("\n1)Play against bot\n2)Play against friend\nChoose: ");
        scanf("%d",&gamemode);

        if(gamemode==1 || gamemode==2)
            break;
    }

    while(1)
    {
        printf("\nStarting position:");
        printf("\n1)Classic chess");
        printf("\n2)Chess960");
        printf("\n3)Custom");
        printf("\nChoose: ");

        scanf("%d",&startpos);

        if(startpos>=1 && startpos<=3)
            break;
    }

    if(gamemode == 1){
        while(1)
        {
            printf("\nYour color:");
            printf("\n1)White");
            printf("\n2)Black");
            printf("\n3)Random");
            printf("\nChoose: ");

            scanf("%d",&color);

            if(color == 3)
            {
                color = (rand() % 2) + 1;
            }

            if(color >= 1 && color <= 2)
                break;
            }
    }
    

    switch(startpos)
    {
        case 1:
            initializeClassic(&board);
            break;
        case 2:
            initialize960(&board);
            createFEN(&board);
            break;
        case 3:
            initializeClassic(&board);
            break;
    }

    //printBoard(&board);

    if(gamemode==1)
        pvb(color);
    else
        pvp();
    return 0;
}