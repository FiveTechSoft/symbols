Logic error: is_valid should accept v in [0, 99] inclusive. Currently rejects 99 (uses v < 99). Fix to v <= 99 (or equivalent). Forbidden: 'v < 99' as sole upper bound.
