# Reports

Rule: **every code change gets a new report file** here, named `NNNN-short-slug.md`
(NNNN = next number). Also update `../CONTEXT.txt` (state, file map, open points) in the same change.
Reports are written for another AI/engineer who has NOT seen
the chat: they must be self-contained and list every nuance.

## Template
    # NNNN — Title
    Date / version
    ## Goal (what the user asked, in their words + interpretation)
    ## What changed (per file: added/modified/removed, and why)
    ## Design decisions & rejected alternatives
    ## Behavior details / nuances / gotchas
    ## Tested vs NOT tested (be honest)
    ## Known issues / limitations
    ## Next steps / open questions
    ## How to run & verify
