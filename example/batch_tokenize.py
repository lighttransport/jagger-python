import jagger

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)

text = """
吾輩は猫である。
名前はまだない。
明日の天気は晴れです。
"""

# optional: set C++ threads(CPU cores) to use
# default: Use all CPU cores.
tokenizer.set_threads(4)

toks_list = tokenizer.tokenize_batch(text)

for toks in toks_list:
    for tok in toks:
        print(tok)

    print("EOS")

