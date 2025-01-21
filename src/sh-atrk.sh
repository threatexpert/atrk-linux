

if [ -z "$brutePsMaxPid" ]; then
  brutePsMaxPid=132000
fi

########

dbg(){
    if [ -z "$DEBUGON" ]; then
        return
    fi
    >&2 echo '  - '"$*"
}
po_append(){
    local newline='
'
    if [ -z "$pendingOutput" ]; then
        pendingOutput="${1}"
    else
        pendingOutput="${pendingOutput}${newline}${1}"
    fi
}
set_add(){
    local pendingOutput=
    local this="$1"
    local item="$2"
    local i

    while IFS= read -r i
    do
        if [ -z "$i" ]; then
            continue
        fi
        if [ "$i" = "$item" ]; then
            output="$this"
            return 1
        fi
    done <<EOF
$this
EOF
    if [ ! -z "$this" ]; then
        pendingOutput="$this"
    fi
    if [ ! -z "$item" ]; then
        po_append "$item"
    fi
    output="$pendingOutput"
    return 0
}
str_cut_getleft(){
    local str="$1"
    local sep="$2"
    output="${str%%"$sep"*}"
}
str_cut_getright(){
    local str="$1"
    local sep="$2"
    output="${str##*"$sep"}"
}
str_drop_tail(){
    local str="$1"
    local sep="$2"
    output="${str%"$sep"}"
}
str_drop_head(){
    local str="$1"
    local sep="$2"
    output="${str#"$sep"}"
}
str_basebame(){
    local str="$1"
    str_cut_getright "$str" /
}
str_dirname(){
    local str="$1"
    local basename=
    str_cut_getright "$str" /
    basename="$output"
    str_drop_tail "$str" "$basename"
    str_drop_tail "$output" /
}
diffs_input(){
    echo "$1"
    echo "21552e15-c081-4e48-9b0d-747de203c68a"
    echo "$2"
}
list_item_notin_2nd_list(){
    output=$(diffs_input  "$1" "$2" | "$atrk" diffs)
}
str_isdigital(){
    [ -n "$1" ] && [ "$1" -eq "$1" ] 2>/dev/null
    if [ $? -ne 0 ]; then
        return 1
    fi
    return 0
}
str_contains(){
  case "$1" in
    *"$2"*)
    return 0
    ;;
  *)
    return 1
    ;;
  esac
}
########

netstatTcplisten(){
    local item
    local ns
    local bindport
    local ports=""
    ns=$("$atrk" listen)
    while IFS= read -r item
    do
        if [ -z "$item" ]; then
            continue
        fi
        str_cut_getright "$item" ":"
        bindport="$output"
        if [ ! -z "$bindport" ]; then
            set_add "$ports" "$bindport"
            ports="$output"
        fi
    done <<EOF
$ns
EOF
    output="$ports"
}
testTcpPorts(){
    local pendingOutput=
    local ip="$1"
    local ports="$2"
    local item
    local res
    while IFS= read -r item
    do
        if [ -z "$item" ]; then
            continue
        fi
        res=$("$atrk" tcp -w5 "$ip" "$item" "$item" 2>/dev/null)
        if [ "$res" = "$item" ]; then
            po_append "$item"
        fi
    done <<EOF
$ports
EOF
    output="$pendingOutput"
}
findHiddenTcpPort(){
    local tcplisten=
    local bruteopen=
    local portSusp
    local portHidden
    bruteopen=$("$atrk" tcp -w5 127.0.0.1 1 65535 2>/dev/null)
    dbg "Found: $bruteopen"
    netstatTcplisten
    tcplisten="$output"
    dbg "netstat: $tcplisten"
    list_item_notin_2nd_list "$bruteopen" "$tcplisten"
    portSusp="$output"
    dbg "portSusp: $portSusp"
    testTcpPorts '127.0.0.1' "$portSusp"
    portHidden="$output"
    dbg "portHidden: $portHidden"
    output="$portHidden"
}

findHiddenUdpPort(){
    local portHidden
    portHidden=$("$atrk" fhudp1 2>/dev/null)
    output="$portHidden"
}

echo_il(){
    echo $1
}

findHiddenTcpPortMain(){
    local result
    echo " * Detecting hidden TCP port..."
    findHiddenTcpPort
    result="$output"
    if [ ! -z "$result" ]; then
        echo "Suspicious TCP Ports Found:"
        echo_il "$result"
    else
        echo "No found hidden tcp ports."
    fi
}

findHiddenUdpPortMain(){
    local result
    echo " * Detecting hidden UDP port..."
    findHiddenUdpPort
    result="$output"
    if [ ! -z "$result" ]; then
        echo "Suspicious UDP Ports Found:"
        echo_il "$result"
    else
        echo "No found hidden udp ports."
    fi
}

########

test1_pids(){
    local id
    for id in $(echo "$1"); do
        if ! str_isdigital "$id" ; then
            continue
        fi
        if [ ! -d "/proc/$id/fd" ]; then
            continue
        fi
        echo $id
    done
}
pslist(){
  local pids1
  pids1=$(cd /proc && echo *)
  test1_pids "$pids1"
}
find_maxpid(){
  local pids2
  local pid2
  local max=0
  pids2=$(pslist)
  for pid2 in $(echo "$pids2"); do
    if [ $pid2 -gt $max ]; then
        max=$pid2
    fi
  done
  echo "$max"
}
get_ptids(){
  local pids2
  local pid2
  local tids
  local tid
  pids2=$(pslist)
  if [ ! -d "/proc/1/task" ]; then
    echo "$pids2"
    return
  fi  
  for pid2 in $(echo "$pids2"); do
      tids="$(cd /proc/$pid2/task/ 2>/dev/null && echo * )"
      for tid in $(echo "$tids"); do
          echo $tid
      done
  done
}
brute1_ps(){
    "$atrk" access '/proc/%d/fd' $1 $2 2>/dev/null
}
brute2_ps(){
    "$atrk" file /proc/ $1 $2 2>/dev/null
}
test2_pids(){
    local id
    local res
    for id in $(echo "$1"); do
        res=$(brute2_ps "$id" "$id")
        if [ "$res" = "$id" ]; then
            echo $id
        fi
    done
}
brute3_ps(){
    "$atrk" kill $1 $2 2>/dev/null
}
test3_pid(){
    local pid3=$1;
    kill -0 "$pid3" >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        return 0
    fi
    return 1
}
test3_pids(){
    local id
    for id in $(echo "$1"); do
        test3_pid "$id"
        if [ $? -eq 0 ]; then
            echo $id
        fi
    done
}

find_hidden_ps(){
  local psNormal
  local psBrute
  local psSusp
  local psHidden
  local pid5
  local bruteMPid="$brutePsMaxPid"
  if [ "$bruteMPid" -eq 0 ]; then
    bruteMPid=$(find_maxpid)
  fi
  psBrute=$(brute1_ps 2 "$bruteMPid")
  psNormal=$(get_ptids)
  list_item_notin_2nd_list "$psBrute" "$psNormal"
  psSusp="$output"
  psHidden=$(test1_pids "$psSusp")
  
  for pid5 in $(echo "$psHidden"); do
      echo "$pid5"
  done
}
find_hidden_ps2(){
  local psNormal
  local psBrute
  local psSusp
  local psHidden
  local pid5
  local bruteMPid="$brutePsMaxPid"
  if [ "$bruteMPid" -eq 0 ]; then
    bruteMPid=$(find_maxpid)
  fi
  psBrute=$(brute2_ps 2 "$bruteMPid")
  psNormal=$(get_ptids)
  list_item_notin_2nd_list "$psBrute" "$psNormal"
  psSusp="$output"
  psHidden=$(test2_pids "$psSusp")
  
  for pid5 in $(echo "$psHidden"); do
      echo "$pid5"
  done
}
find_hidden_ps3(){
  local psNormal
  local psBrute
  local psSusp
  local psHidden
  local pid5
  local bruteMPid="$brutePsMaxPid"
  if [ "$bruteMPid" -eq 0 ]; then
    bruteMPid=$(find_maxpid)
  fi
  psBrute=$(brute3_ps 2 "$bruteMPid")
  psNormal=$(get_ptids)
  list_item_notin_2nd_list "$psBrute" "$psNormal"
  psSusp="$output"
  psHidden=$(test3_pids "$psSusp")
  
  for pid5 in $(echo "$psHidden"); do
      echo "$pid5"
  done
}
print_hidden_ps(){
    local pid6
    for pid6 in $(echo "$1"); do
        echo "PID: $pid6"
        cat /proc/$pid6/comm 2>/dev/null
        echo ""
        cat /proc/$pid6/cmdline 2>/dev/null
        echo ""
    done
}
add_hidden_pid(){
    local this="$1"
    for p in $(echo "$2"); do
        set_add "$this" "$p"
        this="$output"
    done
    echo "$this"
}

findHiddenProcessMain(){
  local psHidden
  local pids=""

  echo  "* Detecting hidden process..."
  psHidden=$(find_hidden_ps)
  if [ ! -z "$psHidden" ]; then
    echo "ps-method1: found:"
    echo_il "$psHidden"
    pids=$(add_hidden_pid "$pids" "$psHidden")
  else
    echo "ps-method1: not found."
  fi
  psHidden=$(find_hidden_ps2)
  if [ ! -z "$psHidden" ]; then
    echo "ps-method2: found:"
    echo_il "$psHidden"
    pids=$(add_hidden_pid "$pids" "$psHidden")
  else
    echo "ps-method2: not found."
  fi
  psHidden=$(find_hidden_ps3)
  if [ ! -z "$psHidden" ]; then
    echo "ps-method3: found:"
    echo_il "$psHidden"
    pids=$(add_hidden_pid "$pids" "$psHidden")
  else
    echo "ps-method3: not found."
  fi

  if [ ! -z "$pids" ]; then
    echo "Suspicious Processes Found:"
    print_hidden_ps "$pids"
  else
    echo "No found hidden processes."
  fi
}

findHiddenFileMain(){
    local res
    local dir
    local found=0
    local rawres="$1"
    local targets="$(echo / /*/ /*/*bin/)"

    echo  "* Detecting hidden file..."
    echo "Target Directoris: $targets"

    sync
    echo 3 > /proc/sys/vm/drop_caches
    for dir in $(echo $targets); do
        if [ "$dir" = "/dev/" ] || [ "$dir" = "/proc/" ] || [ "$dir" = "/selinux/" ] || [ "$dir" = "/sys/" ] || [ "$dir" = "/run/" ] || [ "$dir" = "/tmp/" ]; then
            continue
        fi
        if [ "$rawres" = "1" ]; then
            echo "<---fhf---$dir"
            "$atrk" fhf -f "$dir" -p -v
            echo "---fhf---$dir>"
        else
            res=$("$atrk" fhf -f "$dir" -p -v)
            if str_contains "$res" "(Suspicious!)"; then
                echo "fhf: $dir is suspicious."
                str_cut_getright "$res" "----FHF----"
                echo "$output"
                found=$((found+1))
            fi
        fi
    done
    if [ "$rawres" != "1" ] && [ $found -eq 0 ]; then
        echo "fhf: No found hidden files."
    fi
}

########

main_allin(){
    findHiddenTcpPortMain
    echo
    findHiddenUdpPortMain
    echo
    findHiddenProcessMain
    echo
    findHiddenFileMain "$1"
}

find_atrk_in_current_dir(){
    local curr_dir
    str_dirname "$0"
    curr_dir="$output"

    for file in $(cd "$curr_dir" && echo atrk_*); do
        chmod +x "$curr_dir/$file"
        output=$("$curr_dir/$file" echo a1b2c3d4 2>/dev/null)
        if [ "$output" = "a1b2c3d4" ]; then
            echo "$curr_dir/$file"
            break
        fi
    done
}

atrk_main(){
    echo "atrk-linux v1.0.0"

    atrk="$(find_atrk_in_current_dir)"
    if [ -z "$atrk" ]; then
        echo "atrk not available."
        exit 1
    fi
    main_allin "$1"
}
atrk_main "$1"
