import re
import sys
from lxml import etree

# Read the input file
with open('lang_en.ts', 'r', encoding='utf-8') as file:
    content = file.read()

# Validate before modification
try:
    etree.fromstring(f"<root>{content}</root>")  # Wrap in root element to parse fragment
except etree.ParseError as e:
    print(f"Input XML is invalid: {e}")
    sys.exit(1)

tree = etree.parse('lang_en.ts')
for message in tree.xpath('//message[translation/@type="unfinished"]'):
    source = message.find('source').text
    translation = message.find('translation')
    translation.text = source
    if 'type' in translation.attrib:
        del translation.attrib['type']

# Check for duplicate translation tags
for message in tree.xpath('//message'):
    translations = message.findall('translation')
    if len(translations) > 1:
        source_text = message.find('source').text
        print(f"Warning: Duplicate translation tags detected in message with source: {source_text}")

tree.write('lang_en.ts', encoding='utf-8', xml_declaration=True)

print('English translation file has been finalized.')
