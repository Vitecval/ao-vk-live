"""Conservative whole-owned-batch publication rules, independent of generation."""


def publication_status(plan, generated_ids, retained_ids, deliberately_excluded_ids=()):
    available = set(generated_ids) | set(retained_ids) | set(deliberately_excluded_ids)
    return {key: {'complete': not (set(group['new']) - available),
                  'pending_art_ids': sorted(set(group['new']) - available),
                  'deliberately_excluded_ids': sorted(set(group['new']) & set(deliberately_excluded_ids))}
            for key, group in plan.items()}
