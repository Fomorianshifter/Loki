import hashlib
import os


def get_file_hash(filepath):
  hasher = hashlib.md5()
  try:
    with open(filepath, 'rb') as f:
      buf = f.read()
      hasher.update(buf)
    return hasher.hexdigest()
  except Exception:
    return None


def scan_for_duplicates(root_dir):
  file_hashes = {}
  duplicates = []

  print(f'Scanning directory: {root_dir}...')
  for dirpath, _, filenames in os.walk(root_dir):
    # Skip virtual environments or git internals to save time
    if '.git' in dirpath or 'venv' in dirpath:
      continue
    for filename in filenames:
      filepath = os.path.join(dirpath, filename)
      file_hash = get_file_hash(filepath)

      if file_hash:
        if file_hash in file_hashes:
          duplicates.append((filepath, file_hashes[file_hash]))
        else:
          file_hashes[file_hash] = filepath

  if duplicates:
    print('\n[!] Found duplicate files:')
    for dup, original in duplicates:
      print(f'  Duplicate: {dup}\n  Original:  {original}\n')
  else:
    print('\n[✓] No duplicate files found.')


if __name__ == '__main__':
  scan_for_duplicates('.')
