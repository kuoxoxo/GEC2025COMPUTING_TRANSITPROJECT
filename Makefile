CC = gcc
CFLAGS = -Wall -std=c99
TARGET = gec2025.exe
SRC = gec2025.c

.PHONY: clean run 

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	powershell -Command "if (Test-Path '$(TARGET)') { Remove-Item '$(TARGET)' }"
	@echo Clean completed

run: $(TARGET)
	$(TARGET)
	@echo Run successful

