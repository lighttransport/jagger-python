import os, sys

# emoji > 2.0
import emoji
from kaomoji.kaomoji import Kaomoji


#
# kaomoji
#
lines = open("kaomoji-list.txt", 'r', encoding="utf-8").readlines()

new_section = True
header = None
classifier0 = None
classifier1 = None
classifier2 = None

for line in lines:
    line = line.strip()
    if len(line) == 0:
        new_section = True
        continue

    if new_section:
        tup = line.split(',')
        assert len(tup) == 3
        classifier0 = tup[0].lstrip().rstrip()
        classifier1 = tup[1].lstrip().rstrip()
        classifier2 = tup[2].lstrip().rstrip()
        new_section = False
        continue

    surface = line

    assert classifier0 is not None
    assert classifier1 is not None
    assert classifier2 is not None

    feature_str = "{},*,*,*,顔文字,{},{},{},*,*,{},{},顔文字".format(surface, classifier0, classifier1, classifier2, surface, surface)

    print(feature_str)


#
# emoji
#

for e in emoji.EMOJI_DATA.keys():
    feature_str = "{},*,*,*,絵文字,*,*,*,*,*,{},{},絵文字".format(e, e, e)

    print(feature_str)
