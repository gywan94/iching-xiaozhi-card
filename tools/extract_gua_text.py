#!/usr/bin/env python3
"""Extract plain text from guapages HTML files for ESP32 display."""

import os
import re
from html.parser import HTMLParser

class HTMLTextExtractor(HTMLParser):
    def __init__(self):
        super().__init__()
        self.text_parts = []
        self.in_script = False
        self.in_style = False
        self.skip_tags = {'script', 'style', 'nav', 'table', 'img'}
        
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'):
            self.in_script = True
        if tag == 'br':
            self.text_parts.append('\n')
        if tag == 'p':
            if self.text_parts and not self.text_parts[-1].endswith('\n'):
                self.text_parts.append('\n')
                
    def handle_endtag(self, tag):
        if tag in ('script', 'style'):
            self.in_script = False
        if tag == 'p':
            self.text_parts.append('\n')
            
    def handle_data(self, data):
        if not self.in_script and not self.in_style:
            # Clean up whitespace
            text = data.strip()
            if text:
                self.text_parts.append(text)
                
    def get_text(self):
        text = ''.join(self.text_parts)
        # Clean up multiple newlines
        text = re.sub(r'\n{3,}', '\n\n', text)
        return text.strip()

def extract_gua_text(html_path):
    with open(html_path, 'r', encoding='utf-8') as f:
        html = f.read()
    
    extractor = HTMLTextExtractor()
    extractor.feed(html)
    return extractor.get_text()

def main():
    guapages_dir = r'D:\cursorproject\dayan-xiaozhi2\iching-android\app\src\main\assets\guapages'
    output_file = r'D:\cursorproject\dayan-xiaozhi2\iching-xiaozhi-card\main\generated\gua_details.inc'
    
    # Map gua index to name (we need to build this from iching_data.json)
    # For now, use a simple mapping based on file order
    gua_names = []
    
    # Read iching_data.json to get name mapping
    import json
    with open(r'D:\cursorproject\dayan-xiaozhi2\iching-android\app\src\main\assets\iching_data.json', 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    # Build guayao -> name mapping
    gua_map = {}
    for guayao, name in data['sixteen_four_gua'].items():
        gua_map[guayao] = name
    
    # Generate C++ header
    with open(output_file, 'w', encoding='utf-8') as out:
        out.write('// Auto-generated gua detail texts\n')
        out.write('#pragma once\n')
        out.write('#include <unordered_map>\n')
        out.write('#include <string>\n\n')
        out.write('static const std::unordered_map<std::string, const char*> kGuaDetails = {\n')
        
        for i in range(1, 65):
            html_file = os.path.join(guapages_dir, f'gua_{i:02d}.html')
            if os.path.exists(html_file):
                text = extract_gua_text(html_file)
                # Find gua name from the text (first line usually contains it)
                lines = text.split('\n')
                gua_name = None
                for line in lines[:5]:
                    if '卦' in line and len(line) < 20:
                        gua_name = line.strip()
                        break
                
                if gua_name:
                    # Escape special chars for C++ string
                    text_escaped = text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
                    out.write(f'    {{"{gua_name}", "{text_escaped}"}},\n')
                    print(f'Processed: {gua_name}')
        
        out.write('};\n')
    
    print(f'\nGenerated: {output_file}')

if __name__ == '__main__':
    main()
