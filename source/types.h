#ifndef _TYPES_H_INCLUDED
#define _TYPES_H_INCLUDED






#include "config.h"







#include "extra/bitop.h"

#include <iostream>     
#include <string>       
#include <algorithm>    
#include <limits>		
#include <chrono>       
#include <vector>       
#include <cassert>      

#if defined(_MSC_VER)

#pragma warning(disable: 4127) 
#pragma warning(disable: 4146) 
#pragma warning(disable: 4800) 
#endif










namespace YaneuraOu {

class Position;








constexpr int     int_max   = (std::numeric_limits<int>    ::max)();
constexpr int     int_min   = (std::numeric_limits<int>    ::min)();
constexpr int64_t int64_max = (std::numeric_limits<int64_t>::max)();
constexpr int64_t int64_min = (std::numeric_limits<int64_t>::min)();
constexpr size_t  size_max  = (std::numeric_limits<size_t> ::max)();
constexpr size_t  size_min  = (std::numeric_limits<size_t> ::min)();






enum Color : uint8_t { BLACK = 0, WHITE = 1, COLOR_NB  , COLOR_ZERO = 0,};


constexpr Color operator ~(Color c) { return (Color)(c ^ 1);  }



constexpr bool is_ok(Color c) { return  c < COLOR_NB; }


std::ostream& operator<<(std::ostream& os, Color c);







enum File : int8_t { FILE_1, FILE_2, FILE_3, FILE_4, FILE_5, FILE_6, FILE_7, FILE_8, FILE_9 , FILE_NB , FILE_ZERO=0 };


constexpr bool is_ok(File f) { return FILE_ZERO <= f && f < FILE_NB; }


constexpr File toFile(char c) { return (File)(c - '1'); }




std::string pretty(File f);


static std::ostream& operator<<(std::ostream& os, File f) { os << (char)('1' + f); return os; }







enum Rank : int8_t { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9 , RANK_NB , RANK_ZERO = 0};


constexpr bool is_ok(Rank r) { return RANK_ZERO <= r && r < RANK_NB; }


constexpr bool canPromote(const Color c, const Rank fromOrToRank) {
	
	
	
	
	return static_cast<bool>(0x1c00007u & (1u << ((c << 4) + fromOrToRank)));
}



constexpr Rank relative_rank(Color c, Rank r) { return c == BLACK ? r : (Rank)(8 - r); }


constexpr Rank toRank(char c) { return (Rank)(c - 'a'); }




std::string pretty(Rank r);


static std::ostream& operator<<(std::ostream& os, Rank r) { os << (char)('a' + r); return os; }









enum Square : int8_t
{
	
	
	
	SQ_11, SQ_12, SQ_13, SQ_14, SQ_15, SQ_16, SQ_17, SQ_18, SQ_19,
	SQ_21, SQ_22, SQ_23, SQ_24, SQ_25, SQ_26, SQ_27, SQ_28, SQ_29,
	SQ_31, SQ_32, SQ_33, SQ_34, SQ_35, SQ_36, SQ_37, SQ_38, SQ_39,
	SQ_41, SQ_42, SQ_43, SQ_44, SQ_45, SQ_46, SQ_47, SQ_48, SQ_49,
	SQ_51, SQ_52, SQ_53, SQ_54, SQ_55, SQ_56, SQ_57, SQ_58, SQ_59,
	SQ_61, SQ_62, SQ_63, SQ_64, SQ_65, SQ_66, SQ_67, SQ_68, SQ_69,
	SQ_71, SQ_72, SQ_73, SQ_74, SQ_75, SQ_76, SQ_77, SQ_78, SQ_79,
	SQ_81, SQ_82, SQ_83, SQ_84, SQ_85, SQ_86, SQ_87, SQ_88, SQ_89,
	SQ_91, SQ_92, SQ_93, SQ_94, SQ_95, SQ_96, SQ_97, SQ_98, SQ_99,

	
	SQ_ZERO = 0, SQ_NB = 81,
	SQUARE_NB = SQ_NB,       
	SQ_NONE   = SQ_NB,       
	SQ_NB_PLUS1 = SQ_NB + 1, 

	
	
	SQ_D = +1, 
	SQ_R = -9, 
	SQ_U = -1, 
	SQ_L = +9, 

	
	SQ_RU = int(SQ_U) + int(SQ_R), 
	SQ_RD = int(SQ_D) + int(SQ_R), 
	SQ_LU = int(SQ_U) + int(SQ_L), 
	SQ_LD = int(SQ_D) + int(SQ_L), 
	SQ_RUU = int(SQ_RU) + int(SQ_U), 
	SQ_LUU = int(SQ_LU) + int(SQ_U), 
	SQ_RDD = int(SQ_RD) + int(SQ_D), 
	SQ_LDD = int(SQ_LD) + int(SQ_D), 
};



constexpr bool is_ok(Square sq) { return SQ_ZERO <= sq && sq <= SQ_NB; }


constexpr bool is_ok_plus1(Square sq) { return SQ_ZERO <= sq && sq < SQ_NB_PLUS1; }


constexpr File SquareToFile[SQ_NB_PLUS1] =
{
	FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1, FILE_1,
	FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2, FILE_2,
	FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3, FILE_3,
	FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4, FILE_4,
	FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5, FILE_5,
	FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6, FILE_6,
	FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7, FILE_7,
	FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8, FILE_8,
	FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9, FILE_9,
	FILE_NB, 
};



constexpr File file_of(Square sq) {   return SquareToFile[sq]; }


constexpr Rank SquareToRank[SQ_NB_PLUS1] =
{
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9,
	RANK_NB, 
};



constexpr Rank rank_of(Square sq) {   return SquareToRank[sq]; }


constexpr Square operator | (File f, Rank r) { Square sq = (Square)(f * 9 + r);  return sq; }


constexpr int dist(Square sq1, Square sq2) { return (!is_ok(sq1) || !is_ok(sq2)) ? int_max : (std::max)(abs(file_of(sq1) - file_of(sq2)), abs(rank_of(sq1) - rank_of(sq2))); }


constexpr bool canPromote(const Color c, const Square fromOrTo) {
	ASSERT_LV2(is_ok(fromOrTo));
	return canPromote(c, rank_of(fromOrTo));
}



constexpr bool canPromote(const Color c, const Square from, const Square to)
{
  return canPromote(c, from) || canPromote(c, to);
}


constexpr Square Inv(Square sq) { return (Square)((SQ_NB - 1) - sq); }



constexpr Square Mir(Square sq) { return File(8-(int)file_of(sq)) | rank_of(sq); }


constexpr Square Flip(Square sq) { return (Square)(SQ_NB - sq - 1); }




static std::string pretty(Square sq) { return pretty(file_of(sq)) + pretty(rank_of(sq)); }



static std::ostream& operator<<(std::ostream& os, Square sq) { os << file_of(sq) << rank_of(sq); return os; }

















enum SquareWithWall : int32_t {
	
	SQWW_R = SQ_R - (1 << 9) + (1 << 24), SQWW_U = SQ_U - (1 << 14) + (1 << 19), SQWW_D = -int(SQWW_U), SQWW_L = -int(SQWW_R),
	SQWW_RU = int(SQWW_R) + int(SQWW_U), SQWW_RD = int(SQWW_R) + int(SQWW_D), SQWW_LU = int(SQWW_L) + int(SQWW_U), SQWW_LD = int(SQWW_L) + int(SQWW_D),

	
	SQWW_11 = SQ_11 | (1 << 8)  | (0 << 9)  | (0 << 14)  | (8 << 19)  | (8 << 24) ,

	
	SQWW_BORROW_MASK = (1 << 13) | (1 << 18) | (1 << 23) | (1 << 28),
};


constexpr Square sqww_to_sq(SquareWithWall sqww) { return Square(sqww & 0xff); }


namespace BB_Table { extern SquareWithWall sqww_table[SQ_NB_PLUS1]; }


static SquareWithWall to_sqww(Square sq) { return BB_Table::sqww_table[sq]; }


constexpr bool is_ok(SquareWithWall sqww) { return (sqww & SQWW_BORROW_MASK) == 0; }


static std::ostream& operator<<(std::ostream& os, SquareWithWall sqww) { os << sqww_to_sq(sqww); return os; }








namespace Effect8
{
	
	
	
	enum Directions : uint8_t {
		DIRECTIONS_ZERO  = 0, DIRECTIONS_RU = 1, DIRECTIONS_R = 2, DIRECTIONS_RD = 4,
		DIRECTIONS_U     = 8, DIRECTIONS_D = 16, DIRECTIONS_LU = 32, DIRECTIONS_L = 64, DIRECTIONS_LD = 128,
		DIRECTIONS_CROSS = DIRECTIONS_U  | DIRECTIONS_D  | DIRECTIONS_R  | DIRECTIONS_L,
		DIRECTIONS_DIAG  = DIRECTIONS_RU | DIRECTIONS_RD | DIRECTIONS_LU | DIRECTIONS_LD,
	};

	
	
	
	extern Directions direc_table[SQ_NB_PLUS1][SQ_NB_PLUS1];
	static Directions directions_of(Square sq1, Square sq2) { return direc_table[sq1][sq2]; }

	
	
	enum Direct {
		DIRECT_RU, DIRECT_R, DIRECT_RD, DIRECT_U, DIRECT_D, DIRECT_LU, DIRECT_L, DIRECT_LD,
		DIRECT_NB, DIRECT_ZERO = 0, DIRECT_RUU = 8, DIRECT_LUU, DIRECT_RDD, DIRECT_LDD, DIRECT_NB_PLUS4
	};

	
	static Direct pop_directions(Directions& d) { return (Direct)pop_lsb(d); }

	
	
	static Direct direct_of(Square sq1, Square sq2)
	{
		auto d = directions_of(sq1, sq2);
		ASSERT_LV3(d != DIRECTIONS_ZERO);
		return pop_directions(d);
	}

	
	constexpr Direct operator~(Direct d) {
		
		static_assert(Effect8::DIRECT_R == 1, "");
		static_assert(Effect8::DIRECT_L == 6, "");
		
		
		return Direct(7 - d);
	}

	
	constexpr Directions to_directions(Direct d) { return Directions(1 << d); }

	constexpr bool is_ok(Direct d) { return DIRECT_ZERO <= d && d < DIRECT_NB_PLUS4; }

	
	constexpr SquareWithWall DirectToDeltaWW_[DIRECT_NB] = { SQWW_RU,SQWW_R,SQWW_RD,SQWW_U,SQWW_D,SQWW_LU,SQWW_L,SQWW_LD, };
	constexpr SquareWithWall DirectToDeltaWW(Direct d) {  return DirectToDeltaWW_[d]; }
}





static bool aligned(Square sq1, Square sq2, Square sq3)
{
	auto d1 = Effect8::directions_of(sq1, sq3);
	return d1 && d1 == Effect8::directions_of(sq2, sq3);
}






constexpr int MAX_PLY = MAX_PLY_NUM;


using Depth = int;














constexpr Depth DEPTH_QS = 0;












constexpr Depth DEPTH_UNSEARCHED   = -2;
constexpr Depth DEPTH_NONE         = -3;








enum Bound : uint8_t {
	BOUND_NONE,  
	BOUND_UPPER, 
	BOUND_LOWER, 
	BOUND_EXACT = BOUND_UPPER | BOUND_LOWER 
};





























using Value = int;

constexpr Value VALUE_ZERO     = 0;

constexpr Value VALUE_DRAW     = 0;


constexpr Value VALUE_NONE     = 32002;


constexpr Value VALUE_INFINITE = 32001;



constexpr Value VALUE_MATE             = 32000;

constexpr Value VALUE_MATE_IN_MAX_PLY  =  VALUE_MATE - MAX_PLY;  
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY; 



constexpr Value VALUE_TB                 =  VALUE_MATE_IN_MAX_PLY - 1;
constexpr Value VALUE_TB_WIN_IN_MAX_PLY  =  VALUE_MATE - MAX_PLY;
constexpr Value VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY;





constexpr Value VALUE_SUPERIOR =  VALUE_TB_WIN_IN_MAX_PLY - 1;


constexpr Value VALUE_MAX_EVAL =  VALUE_SUPERIOR;
constexpr Value VALUE_MIN_EVAL = -VALUE_MAX_EVAL;




constexpr Value VALUE_NOT_EVALUATED = INT32_MAX;


constexpr Value mate_in(int ply) { return (Value)(VALUE_MATE - ply); }



constexpr Value mated_in(int ply) { return (Value)(-VALUE_MATE + ply); }



constexpr bool is_valid(Value value) { return value != VALUE_NONE; }


constexpr bool is_win(Value value) {
	assert(is_valid(value));
	return value >= VALUE_TB_WIN_IN_MAX_PLY;
}


constexpr bool is_loss(Value value) {
	assert(is_valid(value));
	return value <= VALUE_TB_LOSS_IN_MAX_PLY;
}


constexpr bool is_decisive(Value value) { return is_win(value) || is_loss(value); }







extern const char* USI_PIECE;


enum PieceType : uint8_t
{
	
	
	NO_PIECE_TYPE, PAWN, LANCE, KNIGHT, SILVER, BISHOP, ROOK, GOLD,
	KING = 8, PRO_PAWN , PRO_LANCE , PRO_KNIGHT , PRO_SILVER , HORSE, DRAGON, QUEEN,

	PIECE_HAND_ZERO = PAWN, 
	PIECE_HAND_NB = KING,   

	PIECE_TYPE_PROMOTE = 8, 
	PIECE_TYPE_NB = 16, 

	

	
	
	GPM_BR = 16,     
	GPM_GBR = 17,     
	GPM_GHD = 18,     
	GPM_GHDK = 19,     

	
	
	ALL_PIECES = 0,			
	GOLDS = QUEEN,			
	HDK,				    
	BISHOP_HORSE,			
	ROOK_DRAGON,			
	SILVER_HDK,				
	GOLDS_HDK,				
	PIECE_BB_NB,			
};


enum Piece : uint8_t
{
	NO_PIECE = 0,

	
	B_PAWN = 1 , B_LANCE, B_KNIGHT, B_SILVER, B_BISHOP, B_ROOK, B_GOLD, B_KING, B_PRO_PAWN, B_PRO_LANCE, B_PRO_KNIGHT, B_PRO_SILVER, B_HORSE, B_DRAGON, B_GOLDS,
	W_PAWN = 17, W_LANCE, W_KNIGHT, W_SILVER, W_BISHOP, W_ROOK, W_GOLD, W_KING, W_PRO_PAWN, W_PRO_LANCE, W_PRO_KNIGHT, W_PRO_SILVER, W_HORSE, W_DRAGON, W_GOLDS,
	PIECE_NB, 
	PIECE_ZERO = 0,

	

	PIECE_PROMOTE = 8, 
	PIECE_WHITE = 16,  
	PIECE_RAW_NB = 8,  
};



static std::string usi_piece(Piece pc) { return (pc & 32) ? "D":""
		  + std::string(USI_PIECE).substr((pc & 31) * 2, 2); }


constexpr Color color_of(Piece pc)
{

	static_assert(PIECE_WHITE == 16 && WHITE == 1 && BLACK == 0, "");
	return (Color)((pc & PIECE_WHITE) >> 4);
}


constexpr PieceType type_of(Piece pc) { return (PieceType)(pc & 15); }



constexpr bool is_non_promotable_piece(Piece pc)
{
	static_assert(GOLD == 7, "GOLD must be 7.");
	return type_of(pc) >= GOLD;
}



constexpr bool is_promoted(Piece pc)
{
	return type_of(pc) >= PRO_PAWN;
}




constexpr PieceType raw_type_of(Piece pc) { return (PieceType)(pc & 7); }




constexpr Piece raw_of(Piece pc) { return (Piece)(pc & ~8); }


constexpr Piece make_piece(Color c, PieceType pt) {  return (Piece)((c << 4) + pt); }


constexpr Piece make_promoted_piece(Piece pc) { return (Piece)(pc | PIECE_PROMOTE); }


constexpr bool has_long_effect(Piece pc) { return (type_of(pc) == LANCE) || (((pc+1) & 6)==6); }



constexpr bool is_ok(Piece pc) { return  pc < PIECE_NB; }



std::string pretty(Piece pc);
static std::string pretty(PieceType pt) { return pretty((Piece)pt); }


static std::string pretty2(Piece pc) { ASSERT_LV1(color_of(pc) == BLACK); auto s = pretty(pc); return s.substr(1, s.length() - 1); }
static std::string pretty2(PieceType pt) { return pretty2((Piece)pt); }



extern std::string PieceToCharBW;


std::ostream& operator<<(std::ostream& os, Piece pc);
static std::ostream& operator<<(std::ostream& os, PieceType pc) { return os << (Piece)pc; }






enum PieceNumber : u8
{
  PIECE_NUMBER_PAWN = 0, PIECE_NUMBER_LANCE = 18, PIECE_NUMBER_KNIGHT = 22, PIECE_NUMBER_SILVER = 26,
  PIECE_NUMBER_GOLD = 30, PIECE_NUMBER_BISHOP = 34, PIECE_NUMBER_ROOK = 36, PIECE_NUMBER_KING = 38,
  PIECE_NUMBER_BKING = 38, PIECE_NUMBER_WKING = 39, 
  PIECE_NUMBER_ZERO = 0, PIECE_NUMBER_NB = 40,
};


constexpr bool is_ok(PieceNumber pn) { return pn < PIECE_NUMBER_NB; }






constexpr int MAX_MOVES = 600;





constexpr size_t make_key(uint64_t seed) {
	return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

class Move;
class Move16;



std::string to_usi_string(Move m);
std::string to_usi_string(Move16 m);















enum MoveEnum : uint32_t {
	MOVE_NONE    = 0,             

	MOVE_NULL    = (1 << 7) + 1,  
	MOVE_RESIGN  = (2 << 7) + 2,  
	MOVE_WIN     = (3 << 7) + 3,  

	MOVE_DROP    = 1 << 14,       
	MOVE_PROMOTE = 1 << 15,       
};



enum MoveType : uint16_t {
    NORMAL,
#if STOCKFISH
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
#else
    PROMOTION     = MOVE_PROMOTE,
    DROP          = MOVE_DROP,
#endif
};


class Move
{
public:
	Move() = default;
	constexpr explicit Move(std::uint32_t d): data(d) {}

	

	
	static constexpr Move none() { return Move(MOVE_NONE); }

	
	static constexpr Move null() { return Move(MOVE_NULL); }

	
	static constexpr Move resign() { return Move(MOVE_RESIGN); }

	
	static constexpr Move win() { return Move(MOVE_WIN); }

	

	
	
	Square from_sq() const {
		ASSERT_LV3(is_ok());
		return Square((data >> 7) & 0x7f);
	}

	
	Square to_sq() const { return Square(data & 0x7f); }

	
	bool is_drop() const { return (data & MOVE_DROP) != 0; }

	
	bool is_promote() const { return (data & MOVE_PROMOTE) != 0; }

	
	
	
	
	PieceType move_dropped_piece() const { return PieceType((data >> 7) & 0x7f); }

	
	Piece moved_after_piece() const { return Piece(data >> 16); }


	
    constexpr MoveType type_of() {
        return MoveType(data & (MOVE_PROMOTE | MOVE_DROP));
    }

	
	
	
	constexpr bool is_ok() const {
		
		
		
		
		
		
		return (data >> 7) != (data & 0x7f);
	}

	

	bool operator == (const Move rhs) const { return this->to_u32() == rhs.to_u32(); }
	bool operator != (const Move rhs) const { return !(*this == rhs); }

	

	
	Move16 to_move16() const;
	constexpr uint16_t to_u16() const { return uint16_t(data); }
	constexpr uint32_t to_u32() const { return uint32_t(data); }
	constexpr explicit operator bool() const { return data != 0; }
	constexpr explicit operator uint32_t() const { return uint32_t(data); }

	
    constexpr std::uint16_t raw() const { return uint16_t(data); }

	

	
	std::string to_usi_string() const { return YaneuraOu::to_usi_string(*this); }

	
	static Move from_string(const Position& pos, const std::string usi_move);

	
	struct MoveHash {
		std::size_t operator()(const Move& m) const { return make_key(m.data); }
	};

protected:
	std::uint32_t data;
};




class Move16
{
public:

	constexpr Move16() : data(0) {}
	explicit constexpr Move16(u16 m) : data(m) {}

	

	
	static constexpr Move16 none() { return Move16(MOVE_NONE); }

	
	static constexpr Move16 null() { return Move16(MOVE_NULL); }

	
	static constexpr Move16 resign() { return Move16(MOVE_RESIGN); }

	
	static constexpr Move16 win() { return Move16(MOVE_WIN); }

	

	constexpr Square from_sq() const { return Square((data >> 7) & 0x7f); }
	constexpr Square to_sq() const { return Square(data & 0x7f); }
	constexpr bool is_drop() const { return (data & MOVE_DROP) != 0; }
	constexpr bool is_promote() const { return (data & MOVE_PROMOTE) != 0; }
	constexpr PieceType move_dropped_piece() const { return PieceType((data >> 7) & 0x7f); }

	
    constexpr std::uint16_t raw() const { return uint16_t(data); }

	constexpr MoveType type_of() { return MoveType(data & (MOVE_PROMOTE | MOVE_DROP)); }
	constexpr bool is_ok() const { return (data >> 7) != (data & 0x7f); }

	

	
	bool operator == (const Move16 rhs) const { return data == rhs.data; }
	bool operator != (const Move16 rhs) const { return !(*this == rhs); }
	bool operator == (const Move rhs) const { return this->to_u16() == rhs.to_u16(); }
	bool operator != (const Move rhs) const { return !(*this == rhs); }

	

	
	
	
	
	constexpr uint16_t to_u16() const { return (u16)data; }

	
	

	

	
	std::string to_usi_string() const { return YaneuraOu::to_usi_string(*this); }

	
	static Move16 from_string(const std::string usi_move);

protected:
	uint16_t data;
};


static std::ostream& operator<<(std::ostream& os, Move m)   { os << to_usi_string(m); return os; }
static std::ostream& operator<<(std::ostream& os, Move16 m) { os << to_usi_string(m); return os; }




static Move16 make_move16(Square from, Square to) { return Move16(u32(to) + (u32(from) << 7)); }
constexpr Move make_move(Square from, Square to, Piece pc ) { return Move(u32(to) + (u32(from) << 7) + (u32(pc) << 16)) ; }
constexpr Move make_move(Square from, Square to, Color us , PieceType pt ) { return Move(u32(to) + u32(from << 7) + (((us ? u32(PIECE_WHITE) : 0) + u32(pt)) << 16)) ; }



static Move16 make_move_promote16(Square from, Square to) { return Move16(u32(to) + (u32(from) << 7) + MOVE_PROMOTE); }
constexpr Move make_move_promote(Square from, Square to , Piece pc) { return Move(u32(to) + (u32(from) << 7) + MOVE_PROMOTE + ((pc | PIECE_PROMOTE) << 16)); }
constexpr Move make_move_promote(Square from, Square to , Color us , PieceType pt) { return Move(u32(to) + (u32(from) << 7) + MOVE_PROMOTE + (((us ? u32(PIECE_WHITE) : 0) + u32(pt | PIECE_PROMOTE)) << 16)); }



static Move16 make_move_drop16(PieceType pt, Square to) { return Move16(u32(to) + (u32(pt) << 7) + MOVE_DROP); }
constexpr Move make_move_drop(PieceType pt, Square to , Color us ) { return Move(u32(to) + (u32(pt) << 7) + MOVE_DROP + (((us ? u32(PIECE_WHITE) : 0) + u32(pt)) << 16)); }






static Move16 reverse_move(Move m) { return make_move16(m.to_sq(), m.from_sq()); }





static Move16 flip_move(Move16 m) {

	return 
		m.is_drop   ()  ? make_move_drop16   (m.move_dropped_piece(), Flip(m.to_sq())):
        m.is_promote()  ? make_move_promote16(Flip(m.from_sq())     , Flip(m.to_sq())):
					      make_move16        (Flip(m.from_sq())     , Flip(m.to_sq()));
}


std::string pretty(Move m);


std::string pretty(Move m, Piece movedPieceType);
static std::string pretty(Move m, PieceType movedPieceType) { return pretty(m, (Piece)movedPieceType); }









enum Hand : uint32_t { HAND_ZERO = 0, };


constexpr int PIECE_BITS[PIECE_HAND_NB] = { 0, 0 , 8 , 12 , 16 , 20 , 24  , 28  };


constexpr Hand PIECE_TO_HAND[PIECE_HAND_NB] = {
	(Hand)0,
	(Hand)(1 << PIECE_BITS[PAWN  ]) ,
	(Hand)(1 << PIECE_BITS[LANCE ]) ,
	(Hand)(1 << PIECE_BITS[KNIGHT]) ,
	(Hand)(1 << PIECE_BITS[SILVER]) ,
	(Hand)(1 << PIECE_BITS[BISHOP]) ,
	(Hand)(1 << PIECE_BITS[ROOK  ]) ,
	(Hand)(1 << PIECE_BITS[GOLD  ]) 
};


constexpr int PIECE_BIT_MASK[PIECE_HAND_NB] = { 0, 31, 7, 7, 7, 3, 3, 7 };

constexpr u32 PIECE_BIT_MASK2[PIECE_HAND_NB] = {
	0,
	PIECE_BIT_MASK[PAWN  ] << PIECE_BITS[PAWN  ],
	PIECE_BIT_MASK[LANCE ] << PIECE_BITS[LANCE ],
	PIECE_BIT_MASK[KNIGHT] << PIECE_BITS[KNIGHT],
	PIECE_BIT_MASK[SILVER] << PIECE_BITS[SILVER],
	PIECE_BIT_MASK[BISHOP] << PIECE_BITS[BISHOP],
	PIECE_BIT_MASK[ROOK  ] << PIECE_BITS[ROOK  ],
	PIECE_BIT_MASK[GOLD  ] << PIECE_BITS[GOLD  ]
};


constexpr u32 HAND_BIT_MASK =
	PIECE_BIT_MASK2[PAWN]   |
	PIECE_BIT_MASK2[LANCE]  |
	PIECE_BIT_MASK2[KNIGHT] |
	PIECE_BIT_MASK2[SILVER] |
	PIECE_BIT_MASK2[BISHOP] |
	PIECE_BIT_MASK2[ROOK]   |
	PIECE_BIT_MASK2[GOLD];


constexpr u32 HAND_BORROW_MASK = (HAND_BIT_MASK << 1) & ~HAND_BIT_MASK;





constexpr int hand_count(Hand hand, PieceType pr) {  return (int)(hand >> PIECE_BITS[pr]) & PIECE_BIT_MASK[pr]; }


constexpr u32 hand_exists(Hand hand, PieceType pr) {  return hand & PIECE_BIT_MASK2[pr]; }


constexpr u32 hand_except_pawn_exists(Hand hand) { return hand & (HAND_BIT_MASK ^ PIECE_BIT_MASK2[PAWN]); }



static uint32_t hand_exists_7bit(Hand h) {
    
    
    return PEXT32(~(HAND_BORROW_MASK - h), HAND_BORROW_MASK);
}


constexpr void add_hand(Hand &hand, PieceType pr) { hand = Hand(hand + PIECE_TO_HAND[pr]); }


constexpr void sub_hand(Hand &hand, PieceType pr) { hand = Hand(hand - PIECE_TO_HAND[pr]); }





constexpr bool hand_is_equal_or_superior(Hand h1, Hand h2) { return ((h1-h2) & HAND_BORROW_MASK) == 0; }


std::ostream& operator<<(std::ostream& os, Hand hand);





#if defined(LONG_EFFECT_LIBRARY)






enum HandKind : uint32_t {
	HAND_KIND_PAWN = 1 << (PAWN - 1), HAND_KIND_LANCE = 1 << (LANCE - 1), HAND_KIND_KNIGHT = 1 << (KNIGHT - 1),
	HAND_KIND_SILVER = 1 << (SILVER - 1), HAND_KIND_BISHOP = 1 << (BISHOP - 1), HAND_KIND_ROOK = 1 << (ROOK - 1), HAND_KIND_GOLD = 1 << (GOLD - 1),
	HAND_KIND_KING = 1 << (KING - 1), HAND_KIND_ZERO = 0,
};




static HandKind toHandKind(Hand h) { return (HandKind)PEXT32(h + HAND_BIT_MASK, HAND_BORROW_MASK); }


constexpr bool hand_exists(HandKind hk, Piece pt) {  return static_cast<bool>(hk & (1 << (pt - 1))); }


constexpr bool hand_exceptPawnExists(HandKind hk) { return hk & ~HAND_KIND_PAWN; }




#endif



extern const std::string StartSFEN;








enum EnteringKingRule
{
	EKR_NONE,            
	EKR_24_POINT,        
	EKR_24_POINT_H,      
	EKR_27_POINT,        
	EKR_27_POINT_H,      
	EKR_TRY_RULE,        
	EKR_NULL,            
};


extern std::vector<std::string> EKR_STRINGS;


extern EnteringKingRule to_entering_king_rule(const std::string& rule);



enum RepetitionState
{
	REPETITION_NONE,     
	REPETITION_WIN,      
	REPETITION_LOSE,     
	REPETITION_DRAW,     
	REPETITION_SUPERIOR, 
	REPETITION_INFERIOR, 
	REPETITION_NB,
};

constexpr bool is_ok(RepetitionState rs) { return REPETITION_NONE <= rs && rs < REPETITION_NB; }


std::string to_usi_string(RepetitionState rs);


std::ostream& operator<<(std::ostream& os, RepetitionState rs);


extern Value drawValueTable[REPETITION_NB][COLOR_NB];
static Value draw_value(RepetitionState rs, Color c) {  return drawValueTable[rs][c]; }





class OptionsMap;

namespace Eval
{



enum BonaPiece : int32_t;

}








namespace Test
{
	class UnitTester;
	void UnitTest(Position& pos,std::istringstream& is);
}






class IEngine;


class ThreadPool;


void run_engine_entry();

} 







template<>
struct std::hash<YaneuraOu::Move16> {
	size_t operator()(const YaneuraOu::Move16& m16) const {
		return std::hash<YaneuraOu::u16>()(m16.to_u16());
	}
};

#include "extra/macros.h"


typedef std::chrono::milliseconds::rep TimePoint;
static_assert(sizeof(TimePoint) == sizeof(int64_t), "TimePoint should be 64 bits");



extern void engine_main();


#endif 
