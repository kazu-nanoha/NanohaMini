NanohaMini
===

# Overview

NanohaMini, a USI shogi(japanese-chess) playing engine derived from Stockfish, a UCI chess playing engine.

「なのはmini」は、USIに対応した将棋プログラムです。
　強いチェスプログラムのStockfishをベースに作成した「なのは」のミニ版です。

# コンパイル方法
・Visual Studio Community 2017 で確認しています

  1. x64 Native tools Command Prompt for VS 2017を起動します。
  2. カレントディレクトリをソース展開したところにします。
  3. nmake -f Makefile.vs とすれば、コンパイルできます。

  ※Cygwin や MSYS2 でも make build とすれば、コンパイルできると思います。

# 使用条件および免責等
GPL V3に従います。

バグは内在していると思いますので、ご指摘いただければありがたいです。
 (なるべくやさしくお願いします)

# 謝辞
* ベースとなったStockfish開発者Marco Costalba, Joona Kiiski, Tord Romstadに感謝します。
* Apery開発者の平岡拓也さんに感謝します。
* gpsfish を参考にしました。TeamGPS各位に感謝します。
* Woodyさんのブログ記事もとても参考にしています。
* れさぴょんも参考にしています。開発者の池さんに感謝します。


# 設定について
将棋所のエンジン管理で設定が変更できます。
  OwnBook          … チェックを入れると定跡データを使います。
  RandomBookSelect … チェックを入れると定跡データをランダムに選択します。
  BookFile         … 定跡データを指定します(デフォルトは book_40.jsk です)
  Threads          … スレッド数を指定します(CPUのコア数を推奨)。
  Hash             … ハッシュサイズを指定します。
  ByoyomiMargin    … 秒読みで思考を指定した時間(ms単位)早めに打ち切ります。
                      (例:500と指定し、秒読み3秒の場合、約2.5秒で指します)

# 履歴
2016/01/04 Ver.0.2.2.1   いろいろバグ修正
2015/12/01 Ver.0.2.2     いろいろバグ修正、なのはnano追加
2014/12/26 Ver.0.2.1.1   ByoyomiMarginの追加、MultiPVの削除、致命的バグ修正
2014/12/23 Ver.0.2.1     初版
