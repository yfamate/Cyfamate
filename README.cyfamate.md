# Cyfamate

将棋エンジン。作者の Python 版エンジン [Pyfamate](https://github.com/yfamate/Pyfamate) の後継で、この repository は Pyfamate の fork。既定ブランチ `cyfamate` に、やねうら王を基にした C++ のエンジンを置いている。

## やねうら王からの主な変更
- **評価関数 GP8**：局面の進行度（GP、game phase）でレイヤースタックを選ぶ NNUE（FT 1536・16・64、GP の境界は nn.bin のヘッダから読む）と、駒の利き（threat）入力。`source/eval/nnue/architectures/cyfamate_gp8.h`、`source/cyfamate/cyf_gp8_threat.h`。
- **Zyfamate の組込み**：単独の詰将棋エンジン [Zyfamate](https://github.com/yfamate/Zyfamate) を借りて常駐させ、通常探索と並走させて root と PV 上の局面の詰み・不詰を探索に返す。
- **探索**：move picker の高速化、多スレッドの改良（スレッドごとの history、深さによる投票）、時間管理（GP・形勢・Sharpness・勢いによる配分。[Pyfamate](https://github.com/yfamate/Pyfamate) から移植）。
- 探索の定数は GitHub 版 V9.80 の値。

## やねうら王について（GPLv3 によるクレジット）
- エンジンの C++ 部分（`source/`・`docs/`・`script/`・`docker/`・`YaneuraOu.sln`・`README.md`・`LICENSE`）は、[やねうら王](https://github.com/yaneurao/YaneuraOu)（yaneurao 氏ほか、GitHub 版 V9.80、commit d47c9bf、2026-09-01）を基にしている。やねうら王は GNU GPL v3 で公開されており、Cyfamate のエンジンも同じ GPLv3（`LICENSE`）で配布する。
- やねうら王の部分は GitHub の公開版だけを使う。やねうら王・Stockfish・tanuki- ほかの元の著作権表示は、各ファイルにそのまま残している。
- 改変：2026-10-14、Cyfamate の作者が、上の「やねうら王からの主な変更」と `source/cyfamate/` の各ファイルの内容を加えた（GPLv3 第5条 a）。変更前の版は yaneurao/YaneuraOu の d47c9bf。
- 配布している実行ファイル（Release）の対応するソースは、このブランチの同じ tag。

## Zyfamate について
詰み探索は、別の repository で公開している単独の USI 詰将棋エンジン [Zyfamate](https://github.com/yfamate/Zyfamate)（GPLv3）の `source/zyfamate_core.h`（commit d20fafa、yfamate/Zyfamate#1・#2）を、変更せずに `source/zyfamate/zyfamate_core.h` へ写して使っている。Cyfamate 側では `ZYFAMATE_HOST_YANEURAOU` を定義し、証明木を探索側へ渡す経路を有効にしている。Zyfamate を直す時は Zyfamate 側で直してから写す。

## Pyfamate からの移植
進行度 GP・Sharpness・Momentum・時間配分は、作者の Python 版エンジン Pyfamate から移植した。Pyfamate の source は `pyfamate/` にあり（この repository は Pyfamate の fork で、Pyfamate の履歴がそのまま親になっている）、各移植部分と移植元の関数の対応は `source/cyfamate/PYFAMATE_PORTS.md`。`pyfamate/` は build に使わず、Pyfamate 自身のライセンス（`pyfamate/LICENSE.txt`）に従う。

## build
単一の翻訳単位にまとめてから build する（`tools-cyfamate/amalgamate.py`）。clang 18 以降（Windows は llvm-mingw）で確認。

```
CXX=clang++ sh tools-cyfamate/build.sh Cyfamate.exe          # GP8 網用（既定）
CXX=clang++ sh tools-cyfamate/build_k3k3.sh Cyfamate_k3k3.exe  # やねうら王標準の SFNN_halfka2_1024_7_64_k3k3 網用（計測用）
```
`-march=native` で build するので、動かす機械で build すること。AVX2 だけの CPU では `-DCYF_TARGET_AVX2` を足す。

## 評価関数
GP8 の網（nn.bin）はこの repository には含めない。k3k3 版は、やねうら王系の k3k3 の nn.bin（例：水匠）をそのまま読む。

## ライセンス
やねうら王と同じ GPLv3（`LICENSE`）。やねうら王・Stockfish・tanuki- ほか、元の著作権表示はそのまま残している。
