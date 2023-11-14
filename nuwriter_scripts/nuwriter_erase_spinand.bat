py -3 nuwriter.py -a ddrimg\enc_ddr3_winbond_512mb.bin
IF %ERRORLEVEL% EQU 0 (
    py -3 nuwriter.py -e spinand all
)
