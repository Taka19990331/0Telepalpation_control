#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_test3\\COPY_REPORT_ja.md"
# trans_ctrl_test3 複製記録

trans_ctrl_test2 の全ファイル（計測データ・キャッシュ・過去資料を含む）をコピー。
制御ロジックとパラメータは複製元から変更していません。

- スケッチ: trans_ctrl_test3.ino
- ダウンロード: download_trans_ctrl_test3.ps1（新規CSVの接頭辞もtest3）
- ビルド・ホストテストの出力先: build/trans_ctrl_test3_*
- READMEと診断手順の実行パスをtest3へ更新。
- 保存確認テストはtest2との同一性確認に更新。
- direction_test.cppのtest1参照は元の制御との比較に必要なため維持。
- IMPLEMENTATION_REPORT_ja.md、REFACTOR_REPORT_ja.md、差分、旧ソース一覧、
  計測データは過去の記録として名前・内容を維持。
- trans_ctrl_test3_all_sources.txt は複製後の現行コードから生成。