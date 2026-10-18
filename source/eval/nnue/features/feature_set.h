
#ifndef CLASSIC_NNUE_FEATURE_SET_H_INCLUDED
#define CLASSIC_NNUE_FEATURE_SET_H_INCLUDED

#include "../../../config.h"

#if defined(EVAL_NNUE)

#include "features_common.h"
#include <array>

namespace YaneuraOu {
namespace Eval::NNUE::Features {

template <typename T, T... Values>
struct CompileTimeList;
template <typename T, T First, T... Remaining>
struct CompileTimeList<T, First, Remaining...> {
  static constexpr bool Contains(T value) {
    return value == First || CompileTimeList<T, Remaining...>::Contains(value);
  }
  static constexpr std::array<T, sizeof...(Remaining) + 1>
      kValues = {{First, Remaining...}};
};
template <typename T, T First, T... Remaining>
constexpr std::array<T, sizeof...(Remaining) + 1>
    CompileTimeList<T, First, Remaining...>::kValues;
template <typename T>
struct CompileTimeList<T> {
  static constexpr bool Contains(T ) {
    return false;
  }
  static constexpr std::array<T, 0> kValues = {{}};
};

template <typename T, typename ListType, T Value>
struct AppendToList;
template <typename T, T... Values, T AnotherValue>
struct AppendToList<T, CompileTimeList<T, Values...>, AnotherValue> {
  using Result = CompileTimeList<T, AnotherValue, Values...>;
};

template <typename T, typename ListType, T Value>
struct InsertToSet;
template <typename T, T First, T... Remaining, T AnotherValue>
struct InsertToSet<T, CompileTimeList<T, First, Remaining...>, AnotherValue> {
  using Result = std::conditional_t<
      CompileTimeList<T, First, Remaining...>::Contains(AnotherValue),
      CompileTimeList<T, First, Remaining...>,
      std::conditional_t<(AnotherValue < First),
          CompileTimeList<T, AnotherValue, First, Remaining...>,
          typename AppendToList<T, typename InsertToSet<
              T, CompileTimeList<T, Remaining...>, AnotherValue>::Result,
              First>::Result>>;
};
template <typename T, T Value>
struct InsertToSet<T, CompileTimeList<T>, Value> {
  using Result = CompileTimeList<T, Value>;
};

template <typename Derived>
class FeatureSetBase {
 public:
  template <typename IndexListType>
  static void AppendActiveIndices(
      const Position& pos, TriggerEvent trigger, IndexListType active[2]) {
    for (const auto perspective : COLOR) {
      Derived::CollectActiveIndices(
          pos, trigger, perspective, &active[perspective]);
    }
  }

  template <typename PositionType, typename IndexListType>
  static void AppendChangedIndices(
      const PositionType& pos, TriggerEvent trigger,
      IndexListType removed[2], IndexListType added[2], bool reset[2]) {
    const auto& dp = pos.state()->dirtyPiece;
    if (dp.dirty_num == 0) return;

    for (const auto perspective : COLOR) {
      reset[perspective] = false;
      switch (trigger) {
        case TriggerEvent::kNone:
          break;
        case TriggerEvent::kFriendKingMoved:
          reset[perspective] =
              dp.pieceNo[0] == PIECE_NUMBER_KING + perspective;
          break;
        case TriggerEvent::kEnemyKingMoved:
          reset[perspective] =
              dp.pieceNo[0] == PIECE_NUMBER_KING + ~perspective;
          break;
        case TriggerEvent::kAnyKingMoved:
          reset[perspective] = dp.pieceNo[0] >= PIECE_NUMBER_KING;
          break;
        case TriggerEvent::kAnyPieceMoved:
          reset[perspective] = true;
          break;
        default:
          ASSERT_LV5(false);
          break;
      }
      if (reset[perspective]) {
        Derived::CollectActiveIndices(
            pos, trigger, perspective, &added[perspective]);
      } else {
        Derived::CollectChangedIndices(
            pos, trigger, perspective,
            &removed[perspective], &added[perspective]);
      }
    }
  }
};

template <typename FirstFeatureType, typename... RemainingFeatureTypes>
class FeatureSet<FirstFeatureType, RemainingFeatureTypes...> :
    public FeatureSetBase<
        FeatureSet<FirstFeatureType, RemainingFeatureTypes...>> {
 private:
  using Head = FirstFeatureType;
  using Tail = FeatureSet<RemainingFeatureTypes...>;

 public:
  static constexpr std::uint32_t kHashValue =
      Head::kHashValue ^ (Tail::kHashValue << 1) ^ (Tail::kHashValue >> 31);
  static constexpr IndexType kDimensions =
      Head::kDimensions + Tail::kDimensions;
  static constexpr IndexType kMaxActiveDimensions =
      Head::kMaxActiveDimensions + Tail::kMaxActiveDimensions;
  using SortedTriggerSet = typename InsertToSet<TriggerEvent,
      typename Tail::SortedTriggerSet, Head::kRefreshTrigger>::Result;
  static constexpr auto kRefreshTriggers = SortedTriggerSet::kValues;

  static std::string GetName() {
    return std::string(Head::kName) + "+" + Tail::GetName();
  }

 private:
  template <typename IndexListType>
  static void CollectActiveIndices(
      const Position& pos, const TriggerEvent trigger, const Color perspective,
      IndexListType* const active) {
    Tail::CollectActiveIndices(pos, trigger, perspective, active);
    if (Head::kRefreshTrigger == trigger) {
      const auto start = active->size();
      Head::AppendActiveIndices(pos, perspective, active);
      for (auto i = start; i < active->size(); ++i) {
        (*active)[i] += Tail::kDimensions;
      }
    }
  }

  template <typename IndexListType>
  static void CollectChangedIndices(
      const Position& pos, const TriggerEvent trigger, const Color perspective,
      IndexListType* const removed, IndexListType* const added) {
    Tail::CollectChangedIndices(pos, trigger, perspective, removed, added);
    if (Head::kRefreshTrigger == trigger) {
      const auto start_removed = removed->size();
      const auto start_added = added->size();
      Head::AppendChangedIndices(pos, perspective, removed, added);
      for (auto i = start_removed; i < removed->size(); ++i) {
        (*removed)[i] += Tail::kDimensions;
      }
      for (auto i = start_added; i < added->size(); ++i) {
        (*added)[i] += Tail::kDimensions;
      }
    }
  }

  friend class FeatureSetBase<FeatureSet>;
  template <typename... FeatureTypes>
  friend class FeatureSet;
};

template <typename FeatureType>
class FeatureSet<FeatureType> : public FeatureSetBase<FeatureSet<FeatureType>> {
 public:
  static constexpr std::uint32_t kHashValue = FeatureType::kHashValue;
  static constexpr IndexType kDimensions = FeatureType::kDimensions;

  static constexpr IndexType kMaxActiveDimensions =
      FeatureType::kMaxActiveDimensions;

  using SortedTriggerSet =
      CompileTimeList<TriggerEvent, FeatureType::kRefreshTrigger>;
  static constexpr auto kRefreshTriggers = SortedTriggerSet::kValues;

  static std::string GetName() {
    return FeatureType::kName;
  }

 private:
  static void CollectActiveIndices(
      const Position& pos, const TriggerEvent trigger, const Color perspective,
      IndexList* const active) {
    if (FeatureType::kRefreshTrigger == trigger) {
      FeatureType::AppendActiveIndices(pos, perspective, active);
    }
  }

  static void CollectChangedIndices(
      const Position& pos, const TriggerEvent trigger, const Color perspective,
      IndexList* const removed, IndexList* const added) {
    if (FeatureType::kRefreshTrigger == trigger) {
      FeatureType::AppendChangedIndices(pos, perspective, removed, added);
    }
  }

  friend class FeatureSetBase<FeatureSet>;
  template <typename... FeatureTypes>
  friend class FeatureSet;
};

} 
} 

#endif  

#endif 
