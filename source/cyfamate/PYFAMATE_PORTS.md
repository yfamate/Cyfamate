# Pyfamate からの移植

Cyfamate の進行度（GP）・Sharpness・時間配分は、作者の Python 版エンジン [Pyfamate](https://github.com/yfamate/Pyfamate) から C++ へ移植したもの。
移植元は同じ repository の `pyfamate/`（Pyfamate commit 649e414 の source。この repository は Pyfamate の fork）の `pyfamate/Pyfamate.py`。行番号はこの commit のもの。

| Cyfamate（C++） | 移植元 `pyfamate/Pyfamate.py` |
|---|---|
| 進行度 GP：`Cyfamate::game_phase`・`GamePhaseParams`・`extract_features`・`game_phase_state`（`source/engine/yaneuraou-engine/yaneuraou-search.h`） | `_calc_game_phase`（16540）、`_calc_state_phase`（16411）、`_game_phase_state_counts_core`（43910）、`_gp_dev_contact_core`（43866）、`king_advance_signal`（44792）、`_GP_INITIAL_SQUARES`（43853）、既定値 `_CONFIG_DEFAULTS`（5210）・`game_phase_fit`（6056） |
| GP8 網の組の選択に使う GP（`source/eval/nnue/architectures/cyfamate_gp8.h`・`source/cyfamate/cyf_gp8_threat.h`） | 上の `_calc_game_phase` と同じ値 |
| Sharpness（`source/cyfamate/cyf_sharpness.h`、yaneuraou-search.h の [CSM]） | `_compute_sharpness`（16910）、`_sharpness_combine`（16888）、`_cp_to_winrate`（16699）、`_CSM_*`（16755） |
| Momentum（yaneuraou-search.h の [CSM]） | `_compute_momentum`（16972） |
| 時間配分 Time Aggression（yaneuraou-search.h の [TA-*]：GP・形勢・γ・Momentum ④b・Sharpness ⑦・ponder ⑥・上下限） | `_compute_go_time_dynamics`（28830）、`_time_aggression_base_gain`（6745）、`_TA_CEIL`（6739）・`_TA_FAC_FLOOR`（6742）、`_calc_slow_mover`（16040） |
| `position sfen <board> ...` の受理（usi） | 局面行の形式（13434・42874） |

ライセンス：`pyfamate/` の中身は Pyfamate 自身のライセンス（`pyfamate/LICENSE.txt`）に従う。上の C++ への移植部分は、Pyfamate の作者が Cyfamate の一部として GPLv3 で提供する。`pyfamate/` は Cyfamate の build には使わない。
