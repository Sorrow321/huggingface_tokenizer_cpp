# HuggingFace WordPiece Tokenizer in C++
This is a C++ implementation of WordPiece (BERT) tokenizer inference.

It expects from you a `.json` file in HuggingFace format that contains all the required information to setup the tokenizer. You can usually download this file from HuggingFace model hub.

## How to use it?

Set the path to your **.json** file when creating the tokenizer:
```
WordPieceTokenizer tokenizer("tokenizer.json");
```
## Build:

This implementation requires the **International Components for Unicode (ICU)** library to handle Unicode. Install it with:
```
sudo apt-get install libicu-dev
```
Compile the tokenizer:
```
g++ tokenizer.cpp -licuuc -o tokenizer
```

## Run:
Create a file `sample_file.txt` that contains all the text that you want to tokenize. Then run the following command to get the indices of the generated tokens on your `stdout`. 
```
./tokenizer sample_file.txt
```

## Testing:
If you would like to compare this tool’s token IDs to those from the native Python HuggingFace implementation automatically, you can do the following:
1. (Optionally) Add your test `.txt` files to `tests/input_texts` folder.
2. Run the following command to see if C++ implementation matches the HuggingFace implementation.
```bash
python run_tests.py
```
If you find an example for which this tool fails, feel free to open an issue, and I'll look into it.