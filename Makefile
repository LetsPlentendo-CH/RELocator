
ifeq ($(OS),Windows_NT)
	EXE_LZ = .\uncompLZ.exe
	EXE_RL = .\relocator.exe
else
	EXE_LZ = ./uncompLZ
	EXE_RL = ./relocator
endif

default:
	gcc uncompLZ.c -o $(EXE_LZ)
	$(EXE_LZ) d_basesNP.rel.LZ d_basesNP.rel
	$(EXE_LZ) d_en_bossNP.rel.LZ d_en_bossNP.rel
	$(EXE_LZ) d_enemiesNP.rel.LZ d_enemiesNP.rel
	$(EXE_LZ) d_profileNP.rel.LZ d_profileNP.rel
	gcc relocator.c -o $(EXE_RL)
	$(EXE_RL) d_profileNP.rel 0x807684c0 d_basesNP.rel 0x8076d680 d_en_bossNP.rel 0x80b1c920 d_enemiesNP.rel 0x809a2ca0