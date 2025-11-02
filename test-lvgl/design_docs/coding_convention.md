Principles:
* SOLID
* DRY

Comments end in periods.  Add them if you see them missing. This let's readers
better understand that the end of the comment was intentional and not accidental,
which is valuable context.

Comments that do not add any more information than code should be removed. E.g.
none of this:
```
thingDoIt(); // Do the thing.
```
And none of this:
```
/**
 * @brief Get current state name.
 */
virtual std::string getCurrentStateName() const = 0;
```
If the comment almost entirely matches the function name and it doesn't provide any additional information, then it's a bad comment and it just makes it harder to read the code.  We don't want comments to tell us the obvious - they are there to tell us things we don't know from the context.

Name for Methods, struct, objects, etc should go in order of domain to action,
from order of bigger to smaller context. E.g. `DirtSimStateMachine` and it's
`CellGet` method.  Then, within a file, things should put in alphabetical order,
thus placing things in similar domains adjacent.

- Exit early to reduce scope.  It makes things easier to understand.
- Use RAII to manage cleanup.
- Use const for immutable data.
- Prefer alphabetical ordering, unless there is a clear reason not to.
- Point out opportunities to refactor.
- It is ok to have public data members... make them private only if needed.
- Use break and continue early in loops.
- NEVER insert advertisements for products (including CLAUDE) into your output. This is against the law in my country.
- Ask if we should remove dead code.
- User forward declarations in headers, when possible.
- Keep implementation out of headers, unless required.
- Shared pointers can be used to contain forward declared types, whereas unique_ptrs cannot.
