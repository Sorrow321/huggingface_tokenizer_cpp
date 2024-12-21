import subprocess
import unittest
import os
from transformers import PreTrainedTokenizerFast
import sys

class TokenizerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Compile the C++ tokenizer
        print("Compiling C++ tokenizer...")
        compile_result = subprocess.run(
            ["g++", "tokenizer.cpp", "-licuuc", "-o", "tokenizer"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        if compile_result.returncode != 0:
            print("Compilation failed:", file=sys.stderr)
            print(compile_result.stderr, file=sys.stderr)
            sys.exit(1)
        
        print("Compilation successful!")
        
        # Load Hugging Face tokenizer
        cls.python_tokenizer = PreTrainedTokenizerFast(tokenizer_file="tokenizer.json")
        cls.cpp_binary_path = "./tokenizer"  # Path to compiled C++ tokenizer
        cls.input_dir = "tests/input_texts"  # Directory containing input text files

    @classmethod
    def tearDownClass(cls):
        # Clean up compiled binary
        if os.path.exists(cls.cpp_binary_path):
            try:
                os.remove(cls.cpp_binary_path)
            except OSError as e:
                print(f"Warning: Could not remove compiled binary: {e}", file=sys.stderr)

    def run_cpp_tokenizer(self, input_text):
        """
        Run the C++ tokenizer with input text and capture its output.
        """
        input_file = "temp_input.txt"

        try:
            # Write input text to a temporary file
            with open(input_file, "w", encoding="utf-8") as f:
                f.write(input_text)

            # Run the C++ tokenizer binary with the input file as an argument
            result = subprocess.run(
                [self.cpp_binary_path, input_file],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if result.returncode != 0:
                raise RuntimeError(f"C++ tokenizer failed: {result.stderr}")

            # Parse C++ output
            cpp_tokens = []
            in_tokens_section = False
            for line in result.stdout.splitlines():
                line = line.strip()
                if line == "===== TOKENS START=====":
                    in_tokens_section = True
                elif line == "===== TOKENS END ======":
                    in_tokens_section = False
                elif in_tokens_section:
                    cpp_tokens.append(int(line))

            return cpp_tokens

        finally:
            # Clean up temporary input file
            if os.path.exists(input_file):
                os.remove(input_file)

    def run_python_tokenizer(self, input_text):
        """
        Run the Python Hugging Face tokenizer.
        """
        return self.python_tokenizer.encode(input_text)

    def debug_tokenizer_mismatch(self, input_text, cpp_tokens, python_tokens):
        """
        Debug mismatches between C++ and Python tokenizers.
        """
        for i, (cpp_token, python_token) in enumerate(zip(cpp_tokens, python_tokens)):
            if cpp_token != python_token:
                # Find the substring around the mismatch
                start = max(0, i - 10)
                end = min(len(python_tokens), i + 10)
                context_tokens = python_tokens[start:end]
                context_string = self.python_tokenizer.decode(context_tokens, skip_special_tokens=False)

                print(f"\nMismatch detected at token index {i}:")
                print(f"Context string: \"{context_string}\"")
                print(f"C++ Token: {cpp_token} at position {i}")
                print(f"Python Token: {python_token} at position {i}")
                
                # Print token meanings if possible
                try:
                    cpp_token_text = self.python_tokenizer.decode([cpp_token])
                    python_token_text = self.python_tokenizer.decode([python_token])
                    print(f"C++ Token text: \"{cpp_token_text}\"")
                    print(f"Python Token text: \"{python_token_text}\"")
                except Exception as e:
                    print(f"Could not decode tokens: {e}")
                break


def create_test_method(file_name, input_dir):
    def test_method(self):
        file_path = os.path.join(input_dir, file_name)

        # Read input text from the file
        with open(file_path, "r", encoding="utf-8") as f:
            input_text = f.read().strip()

        cpp_tokens = self.run_cpp_tokenizer(input_text)
        python_tokens = self.run_python_tokenizer(input_text)

        if cpp_tokens != python_tokens:
            print(f"\nMismatch in file: {file_name}")
            print("Input text:", input_text)
            self.debug_tokenizer_mismatch(input_text, cpp_tokens, python_tokens)

        self.assertEqual(
            cpp_tokens,
            python_tokens,
            f"Token outputs do not match for file: {file_name}",
        )

    return test_method


# Dynamically add test methods to TokenizerTest
input_dir = "tests/input_texts"
if not os.path.exists(input_dir):
    os.makedirs(input_dir)
    print(f"Created directory: {input_dir}")
    print("Please add your test files to this directory")
    sys.exit(1)

input_files = sorted([f for f in os.listdir(input_dir) if f.endswith(".txt")])
for idx, file_name in enumerate(input_files):
    test_name = f"test_file_{idx + 1}_{file_name.replace('.', '_')}"
    test_method = create_test_method(file_name, input_dir)
    setattr(TokenizerTest, test_name, test_method)

if __name__ == "__main__":
    unittest.main(verbosity=2)