#!/usr/bin/env python3
"""Train a tiny, reproducible integer multinomial Bayes intent classifier.

This project-owned synthetic corpus is deliberately small. It is not an LLM.
The output contains learned log likelihoods, not privileged tool instructions.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
LABELS = ("mkdir", "write", "read", "list", "move", "delete", "system")

def train():
    data = json.loads((ROOT / "ai/intents.json").read_text())
    counts = [Counter(re.findall(r"[a-z]+", " ".join(data[label]).lower())) for label in LABELS]
    vocabulary = sorted(set().union(*counts))
    weights = [[round(256 * math.log((count[word] + 1) / (sum(count.values()) + len(vocabulary)))) for count in counts] for word in vocabulary]
    return vocabulary, weights

def render(vocabulary, weights):
    rows = ["/* Generated from ai/intents.json by tools/train_intents.py; do not edit. */", "#ifndef AGENTICOS_INTENT_MODEL_H", "#define AGENTICOS_INTENT_MODEL_H", "struct intent_weight { const char *word; int16_t score[7]; };", "static const struct intent_weight intent_model[] = {"]
    rows += ['    {"' + word + '",{' + ','.join(map(str, row)) + '}},' for word, row in zip(vocabulary, weights)]
    return "\n".join(rows + ["};", "#endif", ""])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    vocabulary, weights = train()
    target = ROOT / "kernel/agent/intent_model.h"
    content = render(vocabulary, weights)
    if args.check:
        assert target.read_text() == content, "intent weights need regeneration"
    else:
        target.write_text(content)
    correct = accepted = 0
    rows = json.loads((ROOT / "ai/heldout.json").read_text())
    for expected, text in rows:
        scores = [0] * 7
        for word in re.findall(r"[a-z]+", text.lower()):
            if word in vocabulary:
                scores = [a+b for a,b in zip(scores, weights[vocabulary.index(word)])]
        order = sorted(range(7), key=scores.__getitem__, reverse=True)
        if scores[order[0]] - scores[order[1]] >= 180:
            accepted += 1
            correct += LABELS[order[0]] == expected
    print(f"Intent model: {len(vocabulary)} words, 56 training phrases; held-out {correct}/{accepted} accepted labels correct, {len(rows)-accepted}/{len(rows)} uncertain requests rejected (synthetic, narrow domain)")
    assert accepted >= 10 and correct == accepted
if __name__ == "__main__":
    main()
