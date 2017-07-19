#!/bin/bash

prog="$PWD/sqysh"

[ -x "$prog" ] || {
	echo >&2 "$prog not found -- do you need to compile it?"
	exit 1
}

success=true

tmpdir="$(mktemp -d)"
[ -d "$tmpdir" ] || exit 1
cd "$tmpdir" || exit 1
trap "cd /; rm -rf $tmpdir" EXIT

script="$PWD/script.sh"

cat > "$script" <<EOF
echo this goes straight to stdout
echo this goes to a file > file_a
tr a b < file_a > file_b
tr b c > file_c < file_b
EOF

mkdir bash sqysh-{script,stdin}

(cd bash && bash "$script" > stdout 2> stderr)
(cd sqysh-script && "$prog" "$script" > stdout 2> stderr)
(cd sqysh-stdin && cat "$script" | "$prog" > stdout 2> stderr)

for t in sqysh-{script,stdin}; do
	if ! diff -r bash "$t" &> /dev/null; then
		echo >&2 "Test '$t' failed"
		success=false
	fi
done

if $success; then
	echo "Basic tests passed!"
	echo "You'll need to do more thorough testing of your own as well though."
	echo "(Use valgrind, fix any errors & leaks it reports!)"
else
	echo "Failing script was this:"
	echo "-----------8<-----------"
	cat "$script"
	echo "----------->8-----------"
	echo "Compare your sqysh behavior with bash on the same commands."
fi
$success
