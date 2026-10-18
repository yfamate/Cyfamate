#include "mate.h"
#if defined (USE_MATE_1PLY)
#include "../position.h"
#include "../movegen.h"

namespace YaneuraOu {
namespace Mate {

void init_mate_1ply();


void init() { init_mate_1ply(); }


Move weak_mate_3ply(const Position& pos, int ply) {
    Move m = Mate::mate_1ply(pos);
    if (m)
        return m;

    if (ply <= 1)
        return Move::none();

    Color    us      = pos.side_to_move();
    Color    them    = ~us;
    Bitboard around8 = kingEffect(pos.square<KING>(them));

    Position* This = ((Position*) &pos);

    StateInfo si;
    StateInfo si2;

    for (auto em : MoveList<CHECKS>(pos))
    {
        Move m = Move(em);

        Square to = m.to_sq();
        if ((around8 & to)

#if !defined(LONG_EFFECT_LIBRARY)
            && (m.is_drop()
                  ? pos.effected_to(us, to)
                  : (bool) (pos.attackers_to(us, to, pos.pieces() ^ m.from_sq()) ^ m.from_sq()))

            && (pos.attackers_to(them, to, pos.pieces()) ^ pos.square<KING>(them))
#else
            && (m.is_drop() ? pos.effected_to(us, to)
                            : pos.board_effect[us].effect(to) >= 2
                                || (pos.long_effect.directions_of(us, m.from_sq())
                                    & Effect8::directions_of(m.from_sq(), to))
                                     != 0)

            && (pos.board_effect[them].effect(to) <= 1)
#endif
        )
        {
            if (!pos.legal(m))
                continue;

            ASSERT_LV3(pos.gives_check(m));

            This->do_move(m, si, true);

            ASSERT_LV3(pos.in_check());

            for (auto m2 : MoveList<EVASIONS>(pos))
            {
                if (!pos.legal(m2))
                    continue;

                if (pos.gives_check(m2))
                    goto NEXT_CHECK;

                This->do_move(m2, si2, false);

                ASSERT_LV3(!pos.in_check());

                if (!weak_mate_3ply(pos, ply - 2))
                {
                    This->undo_move(m2);
                    goto NEXT_CHECK;
                }

                This->undo_move(m2);
            }

            This->undo_move(m);

            return m;

NEXT_CHECK:;
            This->undo_move(m);
        }
    }
    return Move::none();
}

MateRepetitionState mate_repetition(const Position& pos,
                                    const int       plies_from_root,
                                    const int       repetition_ply,
                                    bool            or_node) {



    if (or_node)
    {
        switch (pos.is_repetition(plies_from_root))
        {
        case REPETITION_DRAW :
        case REPETITION_LOSE :
        case REPETITION_INFERIOR :  
            return MateRepetitionState::Mated;  

        case REPETITION_WIN :  
        case REPETITION_SUPERIOR :  
            return MateRepetitionState::Mate;

        case REPETITION_NONE :
            break;

        default :
            UNREACHABLE;
        }

        if (plies_from_root < repetition_ply)
        {

            switch (pos.is_repetition(repetition_ply))
            {
            case REPETITION_DRAW :  
            case REPETITION_LOSE :
                return MateRepetitionState::Mated;  

            case REPETITION_WIN :  
                return MateRepetitionState::Mate;  

            case REPETITION_NONE :
            case REPETITION_INFERIOR :  
            case REPETITION_SUPERIOR :  
                break;

            default :
                UNREACHABLE;
            }
        }
    }
    else
    {

        switch (pos.is_repetition(plies_from_root))
        {
        case REPETITION_DRAW :  
        case REPETITION_WIN :   
        case REPETITION_SUPERIOR: 
            return MateRepetitionState::Mate;  

        case REPETITION_LOSE :  
        case REPETITION_INFERIOR: 
            return MateRepetitionState::Mated;  

        case REPETITION_NONE :
            break;

        default :
            UNREACHABLE;
        }

        if (plies_from_root < repetition_ply)
        {
            switch (pos.is_repetition(repetition_ply))
            {
            case REPETITION_DRAW :
            case REPETITION_WIN :                  
                return MateRepetitionState::Mate;  

            case REPETITION_LOSE :
                return MateRepetitionState::Mated;  

            case REPETITION_NONE :
            case REPETITION_INFERIOR: 
            case REPETITION_SUPERIOR: 
                break;

            default :
                UNREACHABLE;
            }
        }
    }
    return MateRepetitionState::Unknown;
}

#if defined(USE_MATE_SOLVER) || defined(USE_MATE_DFPN)


void MateHashEntry::save(Key                          board_key,
                         Color                        root_color,
                          Hand hand,
                         bool                         is_mate,
                         u32                          ply,
                         Move                         move) {
    lock();


    if (this->board_key == (board_key & 0xffffffffffff) 
        && this->root_color == root_color && this->is_mate == is_mate)
    {

        bool or_node = root_color == side_to_move();

        bool entry_is_better;
        if (is_mate)
        {
            entry_is_better = (or_node && hand_is_equal_or_superior(hand, this->get_hand()))
                           || (!or_node && hand_is_equal_or_superior(this->get_hand(), hand));
        }
        else
        {
            entry_is_better = (or_node && hand_is_equal_or_superior(this->get_hand(), hand))
                           || (!or_node && hand_is_equal_or_superior(hand, this->get_hand()));
        }

        if (entry_is_better)
        {
            unlock();
            return;
        }
    }

    this->board_key  = board_key;
    this->root_color = root_color;
    this->is_mate = is_mate;
    this->ply     = ply;
    this->hand    = (u32) hand;
    this->set_move(move);
    unlock();
}


void MateHashEntry::lock() {
    while (true)
    {
        bool expected = false;
        if (mutex.compare_exchange_weak(expected,  true))
            break;
    }
}

void MateHashEntry::unlock() { mutex = false; }

MateHashEntry* MateHashTable::first_entry(const Key board_key, Color side_to_move) const {
    uint64_t index = mul_hi64((u64) board_key >> 1, entryCount);
    return &table[(index << 1) | side_to_move];
}

void MateHashTable::resize(size_t mbSize) {
    size_t size = mbSize * 1024 * 1024 / 16;

    if (entryCount != size)
    {
        table      = new MateHashEntry[size];
        entryCount = size;

    }
}

void MateHashTable::clear(ThreadPool& threads) {
    Tools::memclear(threads, "MateHash", table, entryCount * sizeof(MateHashEntry));
}


#endif  

}  
} 

#if defined(USE_MATE_DFPN)


#define DFPN32
#include "mate_dfpn.hpp"
#undef DFPN32

#define DFPN64
#include "mate_dfpn.hpp"
#undef DFPN64

namespace YaneuraOu {
namespace {

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode32bitSolver() {
    return std::make_unique<
      Mate::Dfpn32::MateDfpnPn<u32, false , false >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode16bitOrderingSolver() {
    return std::make_unique<
      Mate::Dfpn32::MateDfpnPn<u32, true , false >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode64bitSolver() {
    return std::make_unique<
      Mate::Dfpn64::MateDfpnPn<u64, false , false >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode48bitOrderingSolver() {
    return std::make_unique<
      Mate::Dfpn64::MateDfpnPn<u64, true , false >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode32bitWithHashSolver() {
    return std::make_unique<
      Mate::Dfpn32::MateDfpnPn<u32, false , true >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode16bitOrderingWithHashSolver() {
    return std::make_unique<
      Mate::Dfpn32::MateDfpnPn<u32, true , true >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode64bitWithHashSolver() {
    return std::make_unique<
      Mate::Dfpn64::MateDfpnPn<u64, false , true >>();
}

std::unique_ptr<Mate::Dfpn::MateDfpnSolverInterface> BuildNode48bitOrderingWithHashSolver() {
    return std::make_unique<
      Mate::Dfpn64::MateDfpnPn<u64, true , true >>();
}
}  

namespace Mate::Dfpn {
MateDfpnSolver::MateDfpnSolver(DfpnSolverType t) { ChangeSolverType(t); }

void MateDfpnSolver::ChangeSolverType(DfpnSolverType t) {
    switch (t)
    {
    case DfpnSolverType::None :
        impl = std::unique_ptr<MateDfpnSolverInterface>();
        break;
    case DfpnSolverType::Node32bit :
        impl = BuildNode32bitSolver();
        break;
    case DfpnSolverType::Node16bitOrdering :
        impl = BuildNode16bitOrderingSolver();
        break;
    case DfpnSolverType::Node64bit :
        impl = BuildNode64bitSolver();
        break;
    case DfpnSolverType::Node48bitOrdering :
        impl = BuildNode48bitOrderingSolver();
        break;
    case DfpnSolverType::Node32bitWithHash :
        impl = BuildNode32bitWithHashSolver();
        break;
    case DfpnSolverType::Node16bitOrderingWithHash :
        impl = BuildNode16bitOrderingWithHashSolver();
        break;
    case DfpnSolverType::Node64bitWithHash :
        impl = BuildNode64bitWithHashSolver();
        break;
    case DfpnSolverType::Node48bitOrderingWithHash :
        impl = BuildNode48bitOrderingWithHashSolver();
        break;
    }
}

}  

} 

#endif 

#endif 
