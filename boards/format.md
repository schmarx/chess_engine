# Board file format

The first line in the file indicates which player goes next (1 for white, 2 for black).
The second line in the file indicates whether castling is allowed (this is because for some board states it may be not be possible to tell whether a rook or king has moved). This is indicated by four space-separated numbers: can white castle left, can white castle right, can black castle left, can black castle right, where 0 indicates no, and 1 indicates yes.

After this is the board, where each line represents a board row, and cells are space-separated. Each cell consists of two digits: first the piece number, and second the player color. The colors are labelled as:
 - 0 for no color
 - 1 for white
 - 2 for black

The pieces are labelled as:
 - 0 for no piece
 - 1 for pawn
 - 2 for bishop
 - 3 for knight
 - 4 for rook
 - 5 for queen
 - 6 for king