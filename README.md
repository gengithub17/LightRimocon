# LightRimocon
## OverView
Raspberry Pi上に構築したオリジナルシステム<br>
- 部屋の照明を操作
- Raspiブレッドボード上のボタンと，常時稼働している専用のサーバーから操作可能
- 現在はAndroidで専用アプリ作ってサーバーと通信

## Project Layout
- BackApp/
  - LightRimocon/
    - module/
      - LightRimocon.c : モジュールロードしてスペシャルファイル作成
      - Makefile
      - commands.txt
    - program/
      - light.json
      - light.py : リモコン信号作成
    - src/
      - Server.c : リモコン操作用の通信サーバー
      - irrp.py
      - receive.py
      - command_cheat.txt
    - Server : Server.c
  - applog : ログ出力
