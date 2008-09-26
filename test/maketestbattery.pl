#!/usr/bin/perl

print ("#include \"syn68k_public.h\"\n",
       "#include \"testbattery.h\"\n",
       "\n");

while (<>)
{
    if (/^TEST\s*\(([^,]*),\s*([^,]*),\s*([^,]*),\s*([^,]*),\s*([^\)]*)\s*\)/)
    {
	$decls .= "extern void $1 (uint16 *);\n";
	$array .=  "  { \"$1\", $1, $2, $3, $4, $5 },\n";
    }
}

print ($decls, "\n",
       "const TestInfo test_info[] = {\n",
       $array,
       "  { 0, 0, 0, 0, 0, 0 }\n",
       "};\n");
