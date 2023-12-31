import jagger

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)

text = "吾輩は猫である。名前はまだない。"
toks = tokenizer.tokenize(text)

for tok in toks:
    print(tok.surface(), tok.feature())

    # NOTE: surface() string contains trailing whitespaces.
    # Use split() or rsplit() to strip whitespaces if you dont want it.
    # print("surface", tok.surface().rsplit()[0])

print("EOL")


for tok in toks:
    print(tok.surface())

    # print tag(split feature string by comma)
    #
    # optional: Set quote char(UTF-8 single char) in feature string(CVS line). default '"'
    # set_quote_char() must be called for each Token instance, since
    # tag decomposition from feature string is done on the fly.
    #
    # tok.set_quote_char('\'')
    # tok.set_quote_char('”') # zenkaku-quote

    for i in range(tok.n_tags()):
        print("  tag[{}] = {}".format(i, tok.tag(i)))

print("EOL")
