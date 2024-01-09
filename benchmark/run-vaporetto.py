import time

import vaporetto
import zstandard
import tqdm

dctx = zstandard.ZstdDecompressor()
with open('bccwj-suw+unidic_pos+pron/bccwj-suw+unidic_pos+pron.model.zst', 'rb') as fp:
    with dctx.stream_reader(fp) as dict_reader:
        tokenizer = vaporetto.Vaporetto(dict_reader.read(), predict_tags = True)

lines = open("output-wiki.txt", 'r', encoding='utf8').readlines()

s = time.time()
for line in tqdm.tqdm(lines):
    toks = tokenizer.tokenize(line)

e = time.time()

print("Vaporetto: Total {} secs".format(e - s))



