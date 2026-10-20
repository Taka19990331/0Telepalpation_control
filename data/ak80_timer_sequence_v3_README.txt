Source: C:/Users/User/.codex/attachments/6200da8c-66fc-4287-b784-cbb113947bc0/Pasted text.txt
AK80_TIMER_SEQUENCE_V3,MOTOR,80B,CAN,2,ID,2
READY: s=start 1.0000 Nm for 1.000 s/stage, sequence=500/1000/2000/3000 Hz, x=stop
MEASURE,RAM,8192,WARMUP_MS_PER_STAGE,30,DEADLINE,ONE_PERIOD
STOP,SEQUENCE_COMPLETE
timing.csv: one row per control-cycle record; numeric fields preserve source units.
Elapsed_s: seconds since first recorded TxRequest across all stages.
StageElapsed_s: seconds since first recorded TxRequest of that frequency (after 30 ms warm-up).
summary.csv: original firmware summaries, plus stage completion State.
RTT_us: CAN write request to receive callback, including queueing and interrupt latency.
Period_us: control ISR start interval. ISR_us excludes CAN receive callback execution.
Encoding: UTF-8 BOM; delimiter: comma; decimal point: dot; single header row.
Validation: record counts, sequential indices, OK RTT timestamp differences, mean RTT, CSV read-back row counts.
