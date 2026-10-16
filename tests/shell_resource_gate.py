#!/usr/bin/env python3
"""Resource evidence is native exit zero AND a complete, consistent transcript."""
import argparse
import re
import subprocess
import sys

FINAL = 'WINDOWS_FILE_RESOURCE arms=3; candidate_not_closed=1; owner_unproven=1; complete=1'


def fields(line):
    # Logs contain quoted, backslash-escaped UTF-8 names. Do not unescape paths.
    head = line.split(' name="', 1)[0]
    return dict(re.findall(r'([a-z_]+)=([^\s]+)', head))


def validate(text, returncode):
    def require(ok, why):
        if not ok:
            raise ValueError(why)
    require(returncode == 0, 'native_exit_nonzero')
    require(isinstance(text, str), 'text_required')
    # Reject invalid surrogate code points as well as incorrectly decoded bytes.
    text.encode('utf-8', errors='strict')
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    require(lines and lines[-1] == FINAL and lines.count(FINAL) == 1, 'final_marker')
    require(not any(line.startswith(('PSS_ERROR', 'PSS_OBSERVER')) for line in lines), 'observer_or_capture_error')
    require(not any(line.startswith(('PSS_ARM ', 'PSS_IDENTIFICATION ')) and fields(line).get('complete') != '1' for line in lines), 'incomplete_arm_or_identity')
    controls = [line for line in lines if line.startswith('RESOURCE_CONTROLS ')]
    require(controls == ['RESOURCE_CONTROLS complete=1'], 'controls')
    arm_lines = [fields(line) for line in lines if line.startswith('PSS_ARM ')]
    expect = [('count_only_pipe', '0,3,1,3,1'), ('snapshot_only_no_pipe', '0,0,0,0,0'), ('external_snapshot_pipe', '0,3,1,3,1')]
    require([(a.get('arm'), a.get('deltas')) for a in arm_lines] == expect, 'arms')
    ids = [fields(line) for line in lines if line.startswith('PSS_IDENTIFICATION ')]
    require(len(ids) == 1 and all(ids[0].get(k) == v for k, v in {'arm':'external_snapshot_pipe','new':'3','persistent':'1','candidate_never_closed':'1','complete':'1'}.items()), 'candidate_identification')
    queries = [line for line in lines if line.startswith('RESOURCE_QUERY ') and fields(line).get('context') == 'candidate']
    require(len(queries) == 3, 'candidate_query_count')
    parsed = [fields(line) for line in queries]
    require([q.get('mode') for q in parsed] == ['0','1','2'], 'candidate_query_order')
    for line, q in zip(queries, parsed):
        require(all(q.get(k) == v for k, v in {'attempted':'1','returned':'1','harness_error':'0','pending':'0','timeout':'0','duplicate_error':'0','close':'1'}.items()), 'query_execution_or_cleanup')
        require(q.get('complete') in ('0','1'), 'query_complete_field')
        match = re.fullmatch(r'.* name="((?:[^"\\]|\\.)*)"', line)
        require(match is not None, 'quoted_name')
        # Parse the exact emitted C escaping (backslash, quote, control-byte hex).
        escaped = match.group(1)
        def unescape(value):
            out=[]; i=0
            while i<len(value):
                if value[i]!='\\': out.append(value[i]); i+=1; continue
                require(i+1<len(value),'name_escape')
                if value[i+1] in ('\\','"'):out.append(value[i+1]);i+=2
                elif value[i+1]=='x' and i+3<len(value) and re.fullmatch('[0-9a-f]{2}',value[i+2:i+4]):out.append(chr(int(value[i+2:i+4],16)));i+=4
                else:raise ValueError('name_escape')
            return ''.join(out)
        name=unescape(escaped)
        require(str(len(name.encode('utf-16-le'))//2) == q.get('name_chars'), 'name_length')
        if q['mode']=='1':
            require(q['complete']=='1' and q.get('error')=='0' and name.startswith('\\Device\\') and len(name)>8 and '\0' not in name, 'full_nt_path')
            require(q.get('raw_return')==q.get('name_chars'),'path_return_length')
    # Verify the exact interleaving around each candidate helper/query.
    captures = [(i,fields(line)) for i,line in enumerate(lines) if line.startswith('PSS_CAPTURE ') and fields(line).get('arm') in ('resource_before','resource_after')]
    require([(q.get('arm'),q.get('stage')) for _,q in captures] == [(arm,str(mode)) for mode in range(3) for arm in ('resource_before','resource_after')], 'query_capture_count_order')
    counts=set()
    for i,q in captures:
        require(q.get('complete')=='1' and q.get('walk_status')=='259' and q.get('entries')==q.get('child_count') and q.get('entries','').isdigit(), 'query_capture_incomplete')
        counts.add(q['entries'])
    require(len(counts)==1,'query_target_count_changed')
    for mode,line in enumerate(queries):
        qi=lines.index(line); before=captures[mode*2][0]; after=captures[mode*2+1][0]
        require(before<qi<after,'query_capture_interleave')
        helpers=[fields(x) for x in lines[before+1:qi] if x.startswith('RESOURCE_HELPER ')]
        require(len(helpers)==1 and all(helpers[0].get(k)==v for k,v in {'mode':str(mode),'timeout':'0','duplicate_error':'0','cleanup':'1'}.items()),'helper_cleanup')
    allcaps=[fields(line) for line in lines if line.startswith('PSS_CAPTURE ')]
    require(len(allcaps)==18 and all(q.get('complete')=='1' and q.get('entries')==q.get('child_count') and q.get('walk_status')=='259' for q in allcaps),'all_captures')
    results=[line for line in lines if line.startswith('RESOURCE_RESULT ')]
    require(results==['RESOURCE_RESULT resource=full_path_observed owner=unproven target_stable=1 candidate_not_closed=1'],'resource_result')
    require(lines.index(results[0])>captures[-1][0], 'result_order')
    return True


def run(command, timeout=180):
    try:
        completed=subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        output=completed.stdout.decode('utf-8',errors='strict')
        print(output,end='' if output.endswith('\n') else '\n',flush=True)
        validate(output,completed.returncode)
    except (OSError, subprocess.TimeoutExpired, UnicodeError, ValueError) as exc:
        if isinstance(exc, subprocess.TimeoutExpired) and exc.stdout:
            partial=exc.stdout.decode('utf-8',errors='backslashreplace') if isinstance(exc.stdout,bytes) else exc.stdout
            print(partial,end='' if partial.endswith('\n') else '\n',flush=True)
        print('RESOURCE_GATE blocked=' + type(exc).__name__ + ':' + str(exc),file=sys.stderr)
        return 1
    print('RESOURCE_GATE exit_zero_and_complete_evidence=1')
    return 0


def main():
    if hasattr(sys.stdout, 'reconfigure'):sys.stdout.reconfigure(encoding='utf-8',errors='strict')
    parser=argparse.ArgumentParser()
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    command=args.command
    if command and command[0]=='--':command=command[1:]
    if not command:parser.error('native command required')
    return run(command)


if __name__=='__main__':sys.exit(main())
