#!/usr/bin/ocamlrun /usr/bin/ocaml

let config_name = ref ""

let () = Arg.parse
  [ ]
  (function s -> config_name := s) 
  "./kconfig.ml <config_file>"


type options =
  | Config_Yes of string
  | Config_No of string
  | Config_Module of string
  | Config_Value of string * string
  | Config_Comment of string
  | Config_Empty

let print_option = function
  | Config_Yes s -> Printf.printf "CONFIG_%s=y\n" s
  | Config_No s -> Printf.printf "# CONFIG_%s is not set\n" s
  | Config_Module s -> Printf.printf "CONFIG_%s=m\n" s
  | Config_Value (s,v) -> Printf.printf "CONFIG_%s=%s\n" s v
  | Config_Comment s -> Printf.printf "#%s\n" s
  | Config_Empty -> Printf.printf "\n"
  
exception Comment
  
let parse_line fd =
  let line = input_line fd in
  let len = String.length line in
  if len = 0 then Config_Empty else
  try
    if len <= 9 then raise Comment else 
    match line.[0], line.[1], line.[2], line.[3], line.[4], line.[5], line.[6], line.[7], line.[8] with 
    | '#', ' ', 'C', 'O', 'N', 'F', 'I', 'G', '_' ->
      begin
        try
          let space = String.index_from line 8 ' ' in
	  if String.sub line (space + 1) 10 = "is not set" then 
	    let o = String.sub line 9 (space - 9) in
	    Config_No o
	  else raise Comment
        with Not_found | Invalid_argument "String.sub" -> raise Comment
      end
    | '#', _, _, _, _, _, _, _, _ -> raise Comment
    | 'C', 'O', 'N', 'F', 'I', 'G', _, _, _ ->
      begin
        try
          let equal = String.index_from line 6 '=' in
	  let o = String.sub line 7 (equal - 7) in
	  let v = String.sub line (equal + 1) (len - equal - 1) in
	  match v with
	  | "y" -> Config_Yes o
	  | "m" -> Config_Module o
	  | _ -> Config_Value (o,v)
        with Not_found | Invalid_argument "String.sub" -> raise Comment
      end
    | _ -> raise Comment
  with Comment -> Config_Comment (String.sub line 1 (len - 1))

let rec parse_config fd =
  try 
    print_option (parse_line fd);
    parse_config fd
  with End_of_file -> ()
  
let () =
  if !config_name != "" then
  try
    let config = open_in !config_name in
    parse_config config
  with Sys_error s -> Printf.printf "Error: %s\n" s
