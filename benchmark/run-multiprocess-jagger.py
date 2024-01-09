import jagger
import concurrent.futures
from multiprocessing import cpu_count
import os
import sys
from tqdm import tqdm

model_path = "model/kwdlc/patterns"

tokenizer = jagger.Jagger()
tokenizer.load_model(model_path)
#tokenizer.set_threads(16)

lines = open("output-wiki.txt", 'r', encoding='utf8').readlines()

# Use half of CPU cores
num_process = max(1, cpu_count() // 2)

nlines_per_batch = 1000

def run(lines):
    # TODO: Accept List[str] as input for tokenize_batch
    toks_list = tokenizer.tokenize_batch(''.join(lines))

    # NOTE: Cannot return tokenized result at the moment. List[List[PyToken]]] fails pickle serialization
    # So process toks_list here and convert to pure Python object if you want to return something.
    return None


total_ticks = max(1, len(lines) // nlines_per_batch)
with tqdm(total=total_ticks) as pbar:
    with concurrent.futures.ProcessPoolExecutor(max_workers=num_process) as executor:
        futures = {executor.submit(run, lines[i:i+nlines_per_batch]): i for i in range(0, len(lines), nlines_per_batch)}

        results = {}
        for future in concurrent.futures.as_completed(futures):
            arg = futures[future]
            result = future.result()
            pbar.update(1)
