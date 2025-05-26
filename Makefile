CC = g++
CFLAGS = -Wall -g -Iinclude
OBJDIR = build
SRCDIR = src
BINDIR = .
TARGET = $(BINDIR)/cindris

SOURCES = $(wildcard $(SRCDIR)/*.cc)
OBJECTS = $(patsubst $(SRCDIR)/%.cc, $(OBJDIR)/%.o, $(SOURCES))

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: run, test, clean

run:
	make && ./cindris 

clean:
	rm -rf $(OBJDIR) $(TARGET)
