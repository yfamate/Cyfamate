#ifndef MOVEGEN_H_INCLUDED
#define MOVEGEN_H_INCLUDED

#include <algorithm>  
#include <cstddef>

#include "types.h"





namespace YaneuraOu {

#if STOCKFISH

enum GenType {
    CAPTURES,
    QUIETS,
    EVASIONS,
    NON_EVASIONS,
    LEGAL
};

#else


enum GenType {
    
    
    
    
    
    
    
    

    QUIETS,                 
    CAPTURES,               

    QUIETS_ALL,             
    CAPTURES_ALL,           

    CAPTURES_PRO_PLUS,      
    QUIETS_PRO_MINUS,       

    CAPTURES_PRO_PLUS_ALL,  
    QUIETS_PRO_MINUS_ALL,   

    
    
    

    
    
    

    
    
    
    

    EVASIONS,  
    EVASIONS_ALL,  

    NON_EVASIONS,  
    NON_EVASIONS_ALL,  

    
    LEGAL,      
    LEGAL_ALL,  

    CHECKS,      
    CHECKS_ALL,  

    QUIET_CHECKS,  
    QUIET_CHECKS_ALL,  

    
    
    

    RECAPTURES,      
    RECAPTURES_ALL,  
};
#endif

class Position;









struct ExtMove: public Move {

	int value;  

#if STOCKFISH
    void operator=(Move m) { data = m.raw(); }
#else
    
    
    void operator=(const Move m) { data = m.to_u32(); }
#endif

	

    
    
    

    
    operator float() const = delete;


};


inline bool operator<(const ExtMove& f, const ExtMove& s) { return f.value < s.value; }





template<GenType>
Move* generate(const Position& pos, Move* moveList);

#if !STOCKFISH
std::ostream& operator<<(std::ostream& os, ExtMove m);
#endif










template<GenType T>
struct MoveList {
	

    explicit MoveList(const Position& pos) :
        last(generate<T>(pos, moveList)) {}

	
    const Move* begin() const { return moveList; }

	
    const Move* end() const { return last; }

	
    size_t size() const { return last - moveList; }

	
    
    bool        contains(Move move) const { return std::find(begin(), end(), move) != end(); }

#if !STOCKFISH
	
	
    const Move at(size_t i) const {
        ASSERT_LV3(i < size());
        return begin()[i];
    }
#endif

   private:
    
	
    Move moveList[MAX_MOVES], *last;
};


template<GenType Type>
ExtMove* generateMoves(const Position& pos, ExtMove* moveList)
{
    auto moveList0 = static_cast<Move*>(moveList);
    auto moveList1 = generate<Type>(pos, moveList0);

	

	
	int size = int(moveList1 - moveList0);
	for (int i = size - 1; i >= 0; --i)
		
		moveList[i] = moveList0[i];

	return moveList + size;
}

}  

#endif  
