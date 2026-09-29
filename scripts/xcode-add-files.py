#!/usr/bin/env python3
"""Add source files to the Xcode project's engine libraries.

  scripts/xcode-add-files.py <GroupName> <file> [<file> ...]

<GroupName> is the Xcode group whose path matches the files' folder under
Source_Files (e.g. Misc, RenderOther, Sound). Headers get a file reference;
.cpp/.mm files are also compiled into every target that compiles
shell_options.cpp (libalephone and libalephonesteam). Idempotent.
"""
import hashlib, re, sys

PBX = 'Xcode/AlephOne.xcodeproj/project.pbxproj'

def uid(*parts):
    return hashlib.md5(('durandal-' + '-'.join(parts)).encode()).hexdigest()[:24].upper()

def main():
    group, files = sys.argv[1], sys.argv[2:]
    p = open(PBX).read()
    m = re.search(r'([0-9A-F]{24}) /\* %s \*/ = \{\s*isa = PBXGroup;\s*children = \(\n' % re.escape(group), p)
    if not m:
        sys.exit(f'group {group} not found')
    # Sources phases that build the engine library (identified by shell_options.cpp)
    anchors = re.findall(r'\t\t\t\t([0-9A-F]{24}) /\* shell_options\.cpp in Sources \*/,\n', p)
    for f in files:
        name = f.split('/')[-1]
        ref = uid('ref', group, name)
        if ref in p:
            print(f'{name}: already present'); continue
        kind = 'sourcecode.c.h' if name.endswith('.h') else 'sourcecode.cpp.objcpp' if name.endswith('.mm') else 'sourcecode.cpp.cpp'
        line = f'\t\t{ref} /* {name} */ = {{isa = PBXFileReference; lastKnownFileType = {kind}; path = {name}; sourceTree = "<group>"; }};\n'
        i = p.index('/* Begin PBXFileReference section */\n') + len('/* Begin PBXFileReference section */\n')
        p = p[:i] + line + p[i:]
        m = re.search(r'([0-9A-F]{24}) /\* %s \*/ = \{\s*isa = PBXGroup;\s*children = \(\n' % re.escape(group), p)
        p = p[:m.end()] + f'\t\t\t\t{ref} /* {name} */,\n' + p[m.end():]
        if not name.endswith('.h'):
            for a in anchors:
                bf = uid('bf', group, name, a)
                bline = f'\t\t{bf} /* {name} in Sources */ = {{isa = PBXBuildFile; fileRef = {ref} /* {name} */; }};\n'
                i = p.index('/* Begin PBXBuildFile section */\n') + len('/* Begin PBXBuildFile section */\n')
                p = p[:i] + bline + p[i:]
                entry = f'\t\t\t\t{a} /* shell_options.cpp in Sources */,\n'
                p = p.replace(entry, entry + f'\t\t\t\t{bf} /* {name} in Sources */,\n')
        print(f'{name}: added')
    open(PBX, 'w').write(p)

main()
