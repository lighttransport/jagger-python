import datasets
import tqdm

dss = datasets.load_dataset("range3/wiki40b-ja")
print(dss)


f = open("output-wiki.txt", 'w')

for example in tqdm.tqdm(dss['train']):
    texts = example['text'].split()

    # extract paragraph only.
    in_paragraph = False

    txts_result = []
    for text in texts:
        if in_paragraph:
            text = text.replace("_NEWLINE_", '\n')
            f.write(text + '\n')
            in_paragraph = False

        if text == "_START_PARAGRAPH_":
            in_paragraph = True
