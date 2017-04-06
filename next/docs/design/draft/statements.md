# Variable Definition

```
{names} := {expression}

var {names} := {expression}
```

e.g.

```
!! Define 'i' containing int value
i := 42
print(i)

!! From a record which has unnamed fields (like tuple)
r := {1, true, "aaa"}
i, b, s := r
print(i); print(b); print(s)

!! From a record. Field names must be the same as RHS's ones.
r2 := {age: 12, name: "John"}
age, name := r2
print(age); print(name)

!! Match with names. So below is also OK.
name, age := r2

!! Named record
type Person of {name, age}
p := Person{"John", 12}
age, name := p
print(age); print(name)
```

When `var` is added to the definition, the variable will be mutable.
By default, variables are immutable (like `const` in C).

# Variable Assignment

```
{names} = {expression}
```

e.g.

```
var i := 42
i = 21
```

Assign RHS value to variables specifed at LHS. If there is multiple names at LHS, RHS must be
record which has the same number of fields as names at LHS.

Since it changes the value(s) containd in variable(s), the variables must have been defined as
mutable with `var` keyword before.

# Return

```
ret {expression}[, {expressions...}]
```

e.g.

```
func fib(i) {
    if i <= 1
        ret 1
    end
    ret fib(i - 1) + fib(i - 2)
}

func triple(i) {
    return i, i * 2, i * 3
}

ten, twenty, thirty := triple(10)
```

`ret` returns value(s) from function. When multiple expressions are specified, They will be
returned as record.

# Control Flows

## Branch

```
if {cond} then {statements} else {statements} end
if {cond}
    {statements}
else
    {statements}
end

if {cond} then {statements} end
if {cond}
    {statements}
end
```

e.g.

```
if ok(something)
    print("ok")
end
```

`if` statement. `else` can be omitted.

## Switch

```
case {cond} then {statements}
case {cond}
    {statements}
else
    {statements}
end
```

e.g.

```
case i % 15 == 0
    print("fizzbuzz")
case i % 3  == 0
    print("fizz")
case i % 5  == 0
    print("buzz")
else
    print(i)
end
```

Switch statement to execute one of `case` clauses selectively. `else` can be omitted.

## Match

```
match {expression}
case {pattern} then {statements}
case {pattern}
    {statements}
else
    {statements}
end
```

e.g.

```
match status
case 200 then print("ok")
case 404 then print("not found")
else          print("???")
end

type Person of
    case Student(age)
    case Engineer(skill)
    case Baby

// person := ...

match person
case Student(name, _)
    print("Student"); print(age)
case Engineer(skill)
    print("Engineer"); print(skill)
case Baby
    print("Hello, world")
else
    print("who are you?")
end
```

`match` statement to execute one of `case` clauses selectively matching to each patterns.
`else` clause can be omitted.

## Loop

```
for {cond}
    {statements}
end

for {name} in {array expression}
    {statements}
end
```

e.g.

```
for true
    print("unfinite loop!!")
end

for elem in [1, 2, 3, 4]
    print(elem)
end
```

First form is a 'while' loop and second is a 'foreach' loop.

---
[Top](./README.md)