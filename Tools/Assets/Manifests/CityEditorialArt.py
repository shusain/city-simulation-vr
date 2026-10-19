"""Semantic roles for the approved editorial texture pipeline."""

EDITORIAL_USAGES = {
    'housing': 'newspaper_illustration',
    'commerce': 'newspaper_illustration',
    'automation': 'newspaper_illustration',
    'repairs': 'repair_display',
}


def editorial_entries(document):
    rows = document.get('assets')
    if not isinstance(rows, list) or any(not isinstance(row, dict) for row in rows):
        raise ValueError('Editorial assets must be records with semantic keys and usages')
    keys = [row.get('key') for row in rows]
    if any(not isinstance(key, str) for key in keys) or len(set(keys)) != len(keys) or set(keys) != set(EDITORIAL_USAGES):
        raise ValueError('Editorial keys must uniquely identify housing, commerce, automation and repairs')
    if any(row.get('usage') != EDITORIAL_USAGES[row['key']] for row in rows):
        raise ValueError('Editorial usage does not match its semantic key')
    return {row['key']: row for row in rows}
