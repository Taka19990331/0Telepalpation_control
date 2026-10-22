import serial
import csv
import sys

# --- 設定項目 ---
SERIAL_PORT = 'COM5'  # 環境に合わせて変更してください
BAUD_RATE = 115200
CSV_FILENAME = 'motor_data.csv'

# CSVのヘッダー定義
HEADER = ['Time_ms', 'Position_deg', 'Speed_eRPM', 'Current_A','PD', 'chirp','total',  'Temperature_C', 'Error_Code']

print(f"Teensyのポート {SERIAL_PORT} に接続を試みます...")

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1.0) as ser:
        print("接続成功。データ記録を開始します。Ctrl+Cで終了します。\n")
        
        # CSVファイルを開いてヘッダーを書き込む
        with open(CSV_FILENAME, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(HEADER)
            f.flush() # 即座にディスクに書き込む
            
            print(f"[{CSV_FILENAME}] を作成し、ヘッダーを書き込みました。")
            print("-" * 50)

            while True:
                # シリアルから1行読み込み
                raw_line = ser.readline()
                if not raw_line:
                    continue
                    
                try:
                    line = raw_line.decode('utf-8').strip()
                except UnicodeDecodeError:
                    continue
                
                # 「DATA,」で始まる行だけをパースしてCSVに格納
                if line.startswith("DATA,"):
                    # 'DATA,'の部分を取り除き、カンマで分割
                    data_str = line.replace("DATA,", "")
                    data_fields = data_str.split(",")
                    
                    # CSVファイルに数値データ行として書き込み
                    writer.writerow(data_fields)
                    f.flush() # バッファリングを無効にして即時保存
                    
                # それ以外のメッセージ（デバッグログ等）は画面にそのまま表示
                elif line:
                    print(f"[Teensy Log] {line}")

except serial.SerialException as e:
    print(f"\nエラー: ポート {SERIAL_PORT} が見つからないか、開けません。")
    print(e)
except KeyboardInterrupt:
    print("\nユーザーによる中断: プログラムを安全に終了し、ファイルを保存しました。")
    sys.exit(0)
