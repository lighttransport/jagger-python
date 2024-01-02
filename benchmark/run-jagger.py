import jagger
import tqdm
import time

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)
#tokenizer.set_threads(16)

lines = open("output-wiki.txt", 'r', encoding='utf8').readlines()

s = time.time()
for line in tqdm.tqdm(lines):
    toks = tokenizer.tokenize(line)

e = time.time()
print("Jagger: Total {} secs".format(e - s))

#total_secs = 0
#nlines_per_batch = 1024*128
#for i in tqdm.tqdm(range(0, len(lines), nlines_per_batch)):
#    text = '\n'.join(lines[i:i+nlines_per_batch])
#
#    print("run jagger for {} lines.".format(nlines_per_batch))
#    s = time.time()
#    toks_list = tokenizer.tokenize_batch(text)
#    e = time.time()
#    print("{} secs".format(e - s))
#
#    total_secs += (e - s)
#
#    # print result
#    #for toks in toks_list:
#    #    for tok in toks:
#    #        print(tok.surface(), tok.feature())
# print("Total processing time: {} secs".format(total_secs))
