# HuggingFace WordPiece Tokenizer in C++
Takes as an input .json of the pre-trained tokenizer. Only inference mode is available at the moment.

Change your **.json** path here:
```
WordPieceTokenizer tokenizer("tokenizer.json");
```
# Build:

Requires **International Components for Unicode** library:
```
sudo apt-get install libicu-dev
```
Compile:
```
g++ tokenizer.cpp -licuuc -o tokenizer
```

# Run:
```
./tokenizer
```