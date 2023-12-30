import jagger

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)

text = "吾輩は猫である。名前はまだない。"
toks = tokenizer.tokenize(text)

for tok in toks:
    print(tok.surface(), tok.feature())
print("EOL")

for tok in toks:
    # print tag(split feature() by comma)
    print(tok.surface())
    for i in range(tok.n_tags()):
        print("  tag[{}] = {}".format(i, tok.tag(i)))
print("EOL")
