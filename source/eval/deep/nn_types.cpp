#include "nn_types.h"

#if defined(EVAL_DEEP)

#include <cstring> 
#include <cmath>   

#include "../../usi.h"
#include "../../bitboard.h"

using namespace std;

namespace YaneuraOu {

namespace {


uint16_t MoveLabel[0x10000][COLOR_NB];
}

namespace Eval::dlshogi {


PieceType HandPiece2PieceType[HandPieceNum] = {PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP, ROOK};


#if defined(TRT_NN_FP16)
const DType dtype_zero = __float2half(0.0f);
const DType dtype_one  = __float2half(1.0f);
#endif

namespace {

const std::vector<std::string> MODEL_ARCHITECTURES = {
    MODEL_ARCHITECTURE_WCSC28,
    MODEL_ARCHITECTURE_WCSC35,
};

InputFeatureSpec current_input_feature_spec = {
    DEFAULT_MODEL_ARCHITECTURE,
    (u32)COLOR_NB * MAX_FEATURES1_NUM,
    FEATURES2_NUM_WCSC28,
    false,
};

inline void set_packed_feature(PType* packed_features, const int index) {
    packed_features[index >> 3] |= (1 << (index & 7));
}

} 

const std::vector<std::string>& model_architecture_names() {
    return MODEL_ARCHITECTURES;
}

bool set_model_architecture(const std::string& architecture) {
    if (architecture == MODEL_ARCHITECTURE_WCSC28)
    {
        current_input_feature_spec = {
            architecture,
            (u32)COLOR_NB * MAX_FEATURES1_NUM,
            FEATURES2_NUM_WCSC28,
            false,
        };
        return true;
    }

    if (architecture == MODEL_ARCHITECTURE_WCSC35)
    {
        current_input_feature_spec = {
            architecture,
            (u32)COLOR_NB * MAX_FEATURES1_NUM,
            FEATURES2_NUM_WCSC35,
            true,
        };
        return true;
    }

    return false;
}

const InputFeatureSpec& input_feature_spec() {
    return current_input_feature_spec;
}

#if 0  

	
	
	
	
	void make_input_features(const Position& position, int batch_index, PType* packed_features1, PType* packed_features2)
	{
		int f1idx_b = batch_index * ((int)COLOR_NB * (int)MAX_FEATURES1_NUM * (int)SQ_NB);
		int f2idx_b = batch_index * ((int)MAX_FEATURES2_NUM);
		
		
		{
			
			
			const PType bmask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f };
			const PType emask[8] = { 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00 };
			int f1idx_e = f1idx_b + ((int)COLOR_NB * (int)MAX_FEATURES1_NUM * (int)SQ_NB - 1);
			int f2idx_e = f2idx_b + ((int)MAX_FEATURES2_NUM - 1);
			int f1idx_bu = f1idx_b >> 3;
			int f1idx_bl = f1idx_b & 7;
			int f1idx_eu = f1idx_e >> 3;
			int f1idx_el = f1idx_e & 7;
			int f2idx_bu = f2idx_b >> 3;
			int f2idx_bl = f2idx_b & 7;
			int f2idx_eu = f2idx_e >> 3;
			int f2idx_el = f2idx_e & 7;
			packed_features1[f1idx_bu] &= bmask[f1idx_bl];
			std::fill_n(&packed_features1[f1idx_bu + 1], f1idx_eu - f1idx_bu - 1, (PType)0);
			packed_features1[f1idx_eu] &= emask[f1idx_el];
			packed_features2[f2idx_bu] &= bmask[f2idx_bl];
			std::fill_n(&packed_features2[f2idx_bu + 1], f2idx_eu - f2idx_bu - 1, (PType)0);
			packed_features2[f2idx_eu] &= emask[f2idx_el];
		}

		const Bitboard occupied_bb = position.pieces();

		
		Bitboard attacks[COLOR_NB][PieceTypeNum];
		memset(&attacks, 0, sizeof(attacks));
		

		for (Square sq = SQ_11; sq < SQ_NB; sq++)
		{
			Piece pc = position.piece_on(sq);
			if (pc != NO_PIECE)
				attacks[color_of(pc)][type_of(pc)] |= effects_from(pc, sq, occupied_bb);
		}

		
		for (Color c = BLACK; c < COLOR_NB ; ++c)
		{
			
			Color c2 = (position.side_to_move() == BLACK) ? c : ~c;

			
			Bitboard bb[PieceTypeNum];
			memset(&bb, 0, sizeof(bb));

			for (PieceType pt = PAWN; pt < (u32)PieceTypeNum; ++pt)
				bb[pt] = position.pieces(c , pt);

			for (Square sq = SQ_11; sq < SQ_NB; ++sq)
			{
				
				Square sq2 = (position.side_to_move() == BLACK) ? sq : Flip(sq);

				

				for (PieceType pt = PAWN; pt < (u32)PieceTypeNum; ++pt)
				{
					
					if (bb[pt].test(sq))
					{
						
						int f1idx = f1idx_b + (int)c2 * ((int)MAX_FEATURES1_NUM * (int)SQ_NB) + ((int)pt - 1) * ((int)SQ_NB) + (int)sq2;
						packed_features1[f1idx >> 3] |= (1 << (f1idx & 7));
					}
					
					if (attacks[c][pt].test(sq))
					{
						
						int f1idx = f1idx_b + (int)c2 * ((int)MAX_FEATURES1_NUM * (int)SQ_NB) + (PIECETYPE_NUM + pt - 1) * ((int)SQ_NB) + (int)sq2;
						packed_features1[f1idx >> 3] |= (1 << (f1idx & 7));
					}
				}

				
				const int num = std::min(MAX_ATTACK_NUM, position.attackers_to(c, sq, occupied_bb).pop_count());
				for (int k = 0; k < num; k++)
				{
					
					
					int f1idx = f1idx_b + (int)c2 * ((int)MAX_FEATURES1_NUM * (int)SQ_NB) + (PIECETYPE_NUM + PIECETYPE_NUM + k) * ((int)SQ_NB) + (int)sq2;
					packed_features1[f1idx >> 3] |= (1 << (f1idx & 7));
				}
			}

			
			

			
			
			
			
			Hand hand = position.hand_of(c);
			int p = 0;
			for (int hp = 0; hp < HandPieceNum; ++hp)
			{
				PieceType pt = HandPiece2PieceType[hp];
				int num = std::min(hand_count(hand, pt), MAX_PIECES_IN_HAND[hp]);
				{
					
					int f2idx = f2idx_b + (int)c2 * ((int)MAX_PIECES_IN_HAND_SUM) + p;
					for (int i = 0; i < num; ++i) {
						packed_features2[(f2idx + i) >> 3] |= (1 << ((f2idx + i) & 7));
					}
				}
				p += MAX_PIECES_IN_HAND[hp]; 
			}
		}

		
		if (position.in_check()) {
			
			int f2idx = f2idx_b + (int)MAX_FEATURES2_HAND_NUM;
			packed_features2[f2idx >> 3] |= (1 << (f2idx & 7));
		}
	}

#endif

#if 1  




template<Color SideToMove>
void make_input_features_sub(const Position& position,
                             int             batch_index,
                             PType*          packed_features1,
                             PType*          packed_features2) {
    const auto& spec = input_feature_spec();
    int f1idx_b = batch_index * (int)(spec.features1_channels * (u32)SQ_NB);
    int f2idx_b = batch_index * (int)spec.features2_channels;
    
    
    {
        
        
        const PType bmask[8] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f};
        const PType emask[8] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
        int f1idx_e  = f1idx_b + ((int) spec.features1_channels * (int) SQ_NB - 1);
        int f2idx_e  = f2idx_b + ((int) spec.features2_channels - 1);
        int f1idx_bu = f1idx_b >> 3;
        int f1idx_bl = f1idx_b & 7;
        int f1idx_eu = f1idx_e >> 3;
        int f1idx_el = f1idx_e & 7;
        int f2idx_bu = f2idx_b >> 3;
        int f2idx_bl = f2idx_b & 7;
        int f2idx_eu = f2idx_e >> 3;
        int f2idx_el = f2idx_e & 7;
        packed_features1[f1idx_bu] &= bmask[f1idx_bl];
        std::fill_n(&packed_features1[f1idx_bu + 1], f1idx_eu - f1idx_bu - 1, (PType) 0);
        packed_features1[f1idx_eu] &= emask[f1idx_el];
        packed_features2[f2idx_bu] &= bmask[f2idx_bl];
        std::fill_n(&packed_features2[f2idx_bu + 1], f2idx_eu - f2idx_bu - 1, (PType) 0);
        packed_features2[f2idx_eu] &= emask[f2idx_el];
    }

    
    u8 effect_num[SQ_NB][COLOR_NB] = {};

    
    const Bitboard pieces = position.pieces();

    
    auto pawn_bb = position.pieces(PAWN);

    
    auto pieces_without_pawns = position.pieces() & ~pawn_bb;

    
    auto pawn_black = pawn_bb & position.pieces(BLACK);
    auto pawn_white = pawn_bb & position.pieces(WHITE);

    
    pieces_without_pawns.foreach ([&](Square sq) {
        Piece pc      = position.piece_on(sq);
        auto  attacks = effects_from(pc, sq, pieces);
        Color c       = color_of(pc);

        ;
        if (SideToMove == WHITE)
        {
            c  = ~c;
            sq = Flip(sq);
        }

        {
            
            
            int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) type_of(pc) - 1) * ((int) SQ_NB) + (int) sq;
            set_packed_feature(packed_features1, f1idx);
        }

        
        attacks.foreach ([&](Square to) {
            
            if (SideToMove == WHITE)
                to = Flip(to);

            {
                
                
                int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                          + ((int) PIECETYPE_NUM + (int) type_of(pc) - 1) * ((int) SQ_NB)
                          + (int) to;
                set_packed_feature(packed_features1, f1idx);
            }

            
            effect_num[to][c]++;
        });
    });

    

    pawn_black.foreach ([&](Square sq) {
        if (SideToMove == WHITE)
            sq = Flip(sq);
        Color c = SideToMove;

        
        {
            
            int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PAWN - 1) * ((int) SQ_NB) + (int) sq;
            set_packed_feature(packed_features1, f1idx);
        }

        
        Square to = (Square) (sq + (SideToMove == BLACK ? -1 : +1)) ;
        {
            
            int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PIECETYPE_NUM + (int) PAWN - 1) * ((int) SQ_NB) + (int) to;
            set_packed_feature(packed_features1, f1idx);
        }

        
        effect_num[to][c]++;
    });

    

    pawn_white.foreach ([&](Square sq) {
        if (SideToMove == WHITE)
            sq = Flip(sq);
        Color c = ~SideToMove;

        {
            
            int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PAWN - 1) * ((int) SQ_NB) + (int) sq;
            set_packed_feature(packed_features1, f1idx);
        }
        Square to = (Square) (sq + (SideToMove == BLACK ? +1 : -1)) ;
        {
            
            int f1idx = f1idx_b + (int) c * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PIECETYPE_NUM + (int) PAWN - 1) * ((int) SQ_NB) + (int) to;
            set_packed_feature(packed_features1, f1idx);
        }

        effect_num[to][c]++;
    });

    
    
    for (Square sq = SQ_11; sq < SQ_NB; ++sq)
    {
        
        int num = std::min(MAX_ATTACK_NUM, (int) effect_num[sq][BLACK]);
        for (int k = 0; k < num; k++)
        {
            
            
            int f1idx = f1idx_b + (int) BLACK * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PIECETYPE_NUM + (int) PIECETYPE_NUM + k) * ((int) SQ_NB) + (int) sq;
            set_packed_feature(packed_features1, f1idx);
        }

        
        num = std::min(MAX_ATTACK_NUM, (int) effect_num[sq][WHITE]);
        for (int k = 0; k < num; k++)
        {
            
            int f1idx = f1idx_b + (int) WHITE * ((int) MAX_FEATURES1_NUM * (int) SQ_NB)
                      + ((int) PIECETYPE_NUM + (int) PIECETYPE_NUM + k) * ((int) SQ_NB) + (int) sq;
            set_packed_feature(packed_features1, f1idx);
        }
    }

    
    for (Color c = BLACK; c < COLOR_NB; ++c)
    {
        
        Color c2 = (position.side_to_move() == BLACK) ? c : ~c;
        

        
        
        
        
        Hand hand = position.hand_of(c);
        int  p    = 0;
        for (int hp = 0; hp < HandPieceNum; ++hp)
        {
            PieceType pt  = HandPiece2PieceType[hp];
            const int mp  = MAX_PIECES_IN_HAND[hp];
            int       num = std::min(hand_count(hand, pt), mp);
            
            int f2idx = f2idx_b + (int) c2 * ((int) MAX_PIECES_IN_HAND_SUM) + p;
            for (int i = 0; i < num; ++i)
            {
                set_packed_feature(packed_features2, f2idx + i);
            }

            
            
            
            

            p += MAX_PIECES_IN_HAND
              [hp];  
        }

        if (spec.use_nyugyoku_features)
        {
            const Bitboard ef = enemy_field(c);

            int king_count = 0;
            if (ef & position.square<KING>(c))
            {
                const int f2idx = f2idx_b + (int)MAX_FEATURES2_HAND_NUM + 1
                                + (int)c2 * (int)MAX_FEATURES2_NYUGYOKU_NUM;
                set_packed_feature(packed_features2, f2idx);
                king_count = 1;
            }

            const int own_pieces_count = (position.pieces(c) & ef).pop_count() - king_count;
            const int rest_opp_field_num = 10 - own_pieces_count;
            if (rest_opp_field_num < (int)MAX_NYUGYOKU_OPP_FIELD)
            {
                const int f2idx = f2idx_b + (int)MAX_FEATURES2_HAND_NUM + 1
                                + (int)c2 * (int)MAX_FEATURES2_NYUGYOKU_NUM
                                + 1 + std::max(0, rest_opp_field_num);
                set_packed_feature(packed_features2, f2idx);
            }

            const int own_big_pieces_count = (position.pieces(c, BISHOP_HORSE, ROOK_DRAGON) & ef).pop_count();
            const int own_small_pieces_count = own_pieces_count - own_big_pieces_count;
            const int val = own_small_pieces_count
                          + hand_count(hand, PAWN) + hand_count(hand, LANCE) + hand_count(hand, KNIGHT)
                          + hand_count(hand, SILVER) + hand_count(hand, GOLD)
                          + (own_big_pieces_count + hand_count(hand, BISHOP) + hand_count(hand, ROOK)) * 5;
            const int rest_point = (c == BLACK ? 28 : 27) - val;
            if (rest_point < (int)MAX_NYUGYOKU_SCORE)
            {
                const int f2idx = f2idx_b + (int)MAX_FEATURES2_HAND_NUM + 1
                                + (int)c2 * (int)MAX_FEATURES2_NYUGYOKU_NUM
                                + 1 + (int)MAX_NYUGYOKU_OPP_FIELD + std::max(0, rest_point);
                set_packed_feature(packed_features2, f2idx);
            }
        }
    }

    if (position.in_check())
    {
        
        
        int f2idx = f2idx_b + (int) MAX_FEATURES2_HAND_NUM;
        set_packed_feature(packed_features2, f2idx);
    }
}






void make_input_features(const Position& position,
                         int             batch_index,
                         PType*          packed_features1,
                         PType*          packed_features2) {
    position.side_to_move() == BLACK
      ? make_input_features_sub<BLACK>(position, batch_index, packed_features1, packed_features2)
      : make_input_features_sub<WHITE>(position, batch_index, packed_features1, packed_features2);
}
#endif


void extract_input_features(int        batch_size,
                            PType*     packed_features1,
                            PType*     packed_features2,
                            NN_Input1* features1,
                            NN_Input2* features2) {
    const auto& spec = input_feature_spec();
    int    p1len = batch_size * (int)(spec.features1_channels * (u32)SQ_NB);
    int    p2len = batch_size * (int)spec.features2_channels;
    DType* f1    = (DType*) features1;
    DType* f2    = (DType*) features2;
    for (int i = 0; i < p1len; ++i)
    {
        f1[i] = ((packed_features1[i >> 3] >> (i & 7)) & 1) != 0 ? dtype_one : dtype_zero;
    }
    for (int i = 0; i < p2len; ++i)
    {
        std::fill_n(&f2[i * (int) SQ_NB], (int) SQ_NB,
                    ((packed_features2[i >> 3] >> (i & 7)) & 1) != 0 ? dtype_one : dtype_zero);
    }
}



void init_move_label() {
    
    
    

    
    auto dl_make_move_label = [](Move16 move16, Color color) {
        Square to   = move16.to_sq();
        Square from = move16.from_sq();
        bool   drop = move16.is_drop();

        
        int move_direction_label;
        if (!drop)
        {
            
            if (color == WHITE)
            {
                to   = Flip(to);
                from = Flip(from);
            }

            const div_t to_d   = div(to, 9);
            const int   to_x   = to_d.quot;
            const int   to_y   = to_d.rem;
            const div_t from_d = div(from, 9);
            const int   from_x = from_d.quot;
            const int   from_y = from_d.rem;
            const int   dir_x  = from_x - to_x;
            const int   dir_y  = to_y - from_y;

            MOVE_DIRECTION move_direction =
              MOVE_DIRECTION_NONE;  
            if (dir_y < 0 && dir_x == 0)
            {
                move_direction = UP;
            }
            else if (dir_y == -2 && dir_x == -1)
            {
                move_direction = UP2_LEFT;
            }
            else if (dir_y == -2 && dir_x == 1)
            {
                move_direction = UP2_RIGHT;
            }
            else if (dir_y < 0 && dir_x < 0)
            {
                move_direction = UP_LEFT;
            }
            else if (dir_y < 0 && dir_x > 0)
            {
                move_direction = UP_RIGHT;
            }
            else if (dir_y == 0 && dir_x < 0)
            {
                move_direction = LEFT;
            }
            else if (dir_y == 0 && dir_x > 0)
            {
                move_direction = RIGHT;
            }
            else if (dir_y > 0 && dir_x == 0)
            {
                move_direction = DOWN;
            }
            else if (dir_y > 0 && dir_x < 0)
            {
                move_direction = DOWN_LEFT;
            }
            else if (dir_y > 0 && dir_x > 0)
            {
                move_direction = DOWN_RIGHT;
            }

            
            if (move16.is_promote())
            {
                move_direction = MOVE_DIRECTION_PROMOTED[move_direction];
            }
            move_direction_label = move_direction;
        }
        
        else
        {
            
            if (color == WHITE)
                to = Flip(to);
            PieceType pt         = move16.move_dropped_piece();
            const int hand_piece = int(
              pt
              - PAWN);  
            move_direction_label = MOVE_DIRECTION_NUM + hand_piece;
        }

        return 9 * 9 * move_direction_label + to;
    };

    for (auto color : COLOR)
        
        for (Square from_sq = SQ_ZERO; from_sq < SQ_NB + 7; ++from_sq)
            for (auto to_sq : SQ)
                
                for (int promote = 0; promote < 2; ++promote)
                {
                    
                    bool drop = from_sq >= SQ_NB;

                    
                    if (drop && promote)
                        continue;

                    Move16 move;
                    if (!drop)
                    {
                        move = !promote ? make_move16(from_sq, to_sq)
                                        : make_move_promote16(from_sq, to_sq);
                    }
                    else
                    {
                        PieceType pt = (PieceType) (from_sq - (int) SQ_NB + PAWN);
                        move         = make_move_drop16(pt, to_sq);
                    }

                    
                    MoveLabel[move.to_u16()][color] = dl_make_move_label(move, color);
                }

#if 0
		std::set<u16> s;
		for (auto c : COLOR)
			for (u32 i = 0; i < 0x10000; ++i)
				s.insert(MoveLabel[i][c]);
		std::cout << s.size() << std::endl;
		
		
#endif
}


int make_move_label(Move move, Color color) { return MoveLabel[move.to_u16()][color]; }












constexpr float default_softmax_temperature = 1.0f;
float           beta                        = 1.0f / default_softmax_temperature;

void set_softmax_temperature(const float temperature) { beta = 1.0f / temperature; }

void softmax_temperature_with_normalize(std::vector<float>& log_probabilities) {

    

    float max = numeric_limits<float>::min();
    for (float& x : log_probabilities)
    {
        x *= beta;
        if (x > max)
        {
            max = x;
        }
    }

    

    

    float sum = 0.0f;
    for (float& x : log_probabilities)
    {
        x = expf(x - max);
        sum += x;
    }
    
    for (float& x : log_probabilities)
    {
        x /= sum;
    }
}




float cp_to_value(const Value score, const float eval_coef) {
    return 1.0f / (1.0f + expf(-(float) score / eval_coef));
}









Value value_to_cp(const float score, const float eval_coef) {
    int cp;
    if (score == 1.0f)
        cp = 30000;
    else if (score == 0.0f)
        cp = -30000;
    else
    {
        cp = (int) (-logf(1.0f / score - 1.0f) * eval_coef);

        
        cp = std::clamp(cp, -29900, +29900);
    }

    return (Value) cp;
}



void init()
{
    
    static bool initialized = [] {
        
        dlshogi::init_move_label();
        return true;
    }();
}

} 
} 

#endif 
