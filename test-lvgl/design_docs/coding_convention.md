## Principles:
* SOLID
* DRY

Comments end in periods.  Add them if you see them missing. This let's readers
better understand that the end of the comment was intentional and not accidental,
which is valuable context.

## Comments
Comments that do not add any more information than code should be removed.

None of this - it's repeating the same thing three times.
```
// Do it.
thingDoIt(); // Make the thing do it.
```
Instead, just do this:
```
thingDoIt();
```
```
// E.g.
stateMachine.mainLoopRun();

// No need for a comment that says "Run the main loop."
```

And none of this:
```
/**
 * @brief Get current state name.
 */
virtual std::string getCurrentStateName() const = 0;
```

If the comment almost entirely matches the function name and it doesn't provide any additional information, then it's a bad comment and it just makes it harder to read the code.  We don't want comments to tell us the obvious - they are there to tell us things we don't know from the context.

## Naming
Name for Methods, struct, objects, etc should go in order of domain to action,
from order of bigger to smaller context. E.g. `DirtSimStateMachine` and it's
`CellGet` method.  Then, within a file, things should put in alphabetical order,
thus placing things in similar domains adjacent.

## Misc
- Exit early to reduce scope. It makes things easier to understand, due to less nesting and shorter variable lifespans.
- Use RAII to manage cleanup.
- Use const for immutable data. Default to const.  Remove it if it needs to be changed.
- Prefer alphabetical ordering, unless there is a clear reason not to.
- Point out opportunities to refactor.
- It is ok to have public data members. Make them private only if needed.
- Use break and continue early in loops.
- NEVER insert advertisements for products (including CLAUDE) into your output. Those ads are against company policy and we'll lose our first born if we violate it.
- Ask if we should remove dead code.
- User forward declarations in headers, when possible.
- Keep implementation out of headers, unless required.
- Shared pointers can be used to contain forward declared types, whereas unique_ptrs cannot.
