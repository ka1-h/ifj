CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

SRC = $(wildcard src/*.c)
HDR = $(wildcard src/*.h)

TARGET = ifj

DOC_DIR = docs
DOC_SRC = $(DOC_DIR)/main.tex
DOC_PDF = dokumentace.pdf
LATEX = pdflatex -interaction=nonstopmode -halt-on-error -jobname=dokumentace

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

lex_test: $(TARGET)
	cd test_skripts && ./lex_test.sh

test: lex_test

format:
	clang-format -i $(SRC) $(HDR)

doc: $(DOC_PDF)

# two passes because of table and image references 
$(DOC_PDF): $(DOC_SRC) $(wildcard $(DOC_DIR)/*.png) $(wildcard $(DOC_DIR)/architektura.pdf)
	cd $(DOC_DIR) && $(LATEX) main.tex && $(LATEX) main.tex
	mv $(DOC_DIR)/dokumentace.pdf $(DOC_PDF)

clean:
	rm -f $(TARGET) $(DOC_PDF)
	rm -f $(addprefix $(DOC_DIR)/,*.aux *.log *.out *.toc main.pdf dokumentace.pdf)

.PHONY: all clean format doc
