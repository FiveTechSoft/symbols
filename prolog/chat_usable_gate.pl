% chat_usable_gate.pl — el chat aprende palabras del uso, no de listas.
% Uso: swipl -s chat_usable_gate.pl -g usable_gate -t halt
% Palabras de contenido (abrio, puerta, nino, pan, ...) SOLO entran
% ensenadas en el dialogo; el gate no inyecta lexico.
:- encoding(utf8).
:- consult('chat.pl').

usable_gate :-
    bb_load('demo.knowledge.pl'),
    ( catch(run_cases, E,
            ( format('FAIL exception: ~w~n', [E]), halt(1) )) ->
        writeln('USABLE GATE PASS')
    ; writeln('USABLE GATE FAIL'), halt(1)
    ).

run_cases :-
    % P1 intacto: hechos curados siguen respondiendo.
    expect("Who opened door?", "ana"),
    % Verbo novel SIN punto: el acto no-pregunta es ensenar (SVO).
    expect_all("Ana abrio puerta", ["Learned", "abrio"]),
    % Pregunta SIN interrogativo de lista: ancla rel+obj del mapa vivo.
    expect("abrio puerta?", "ana"),
    expect("abrio la puerta?", "ana"),
    expect("ana abrio?", "puerta"),
    % Hueco que el mapa no cubre: unknown, y '?' no ingiere.
    % C1: la pregunta en superficie se guarda (sin lexico).
    capture("abrio ventana?", OutQ),
    \+ sub_string(OutQ, _, _, _, "Learned"),
    \+ memory_relation(_, abrio, ventana, _, _),
    ( sub_string(OutQ, _, _, _, "GAP open") -> true
    ; format('FAIL expected GAP open in ~q~n  got: ~w~n',
             ["abrio ventana?", OutQ]), fail
    ),
    findall(G1, gap(G1, _, open, _, _), Open1),
    length(Open1, 1),
    % eñe y ¿¡ son escritura, no un idioma que se rechace.
    expect("Niño come pan", "Learned"),
    expect("come pan?", "niño"),
    % Alias ensenado (D9): el mapeo se APRENDE, no viene de una lista.
    expect_all("abrio means opened", ["Learned", "means"]),
    expect("abrio door?", "ana"),
    % Lenguaje natural: la respuesta es la tripla dicha, no un nombre suelto.
    expect("abrio puerta?", "ana abrio puerta"),
    % Articulo desconocido pegado a entidad conocida: no es sujeto.
    expect_all("queen plays croquet", ["Learned", "queen"]),
    capture("The queen plays croquet", OutThe),
    \+ sub_string(OutThe, _, _, _, "the --"),
    \+ memory_relation(the, _, _, _, _),
    expect("plays croquet?", "queen plays croquet"),
    expect("Did queen play croquet?", "Yes"),
    expect("And the queen?", "queen plays"),
    % Spans del mapa vivo: "white rabbit" = white_rabbit ya ensenado.
    expect_all("alice followed white_rabbit", ["Learned"]),
    expect("followed white rabbit?", "alice followed white rabbit"),
    % Relacion sola: "who fell?" ancla el verbo vivo, sin objeto.
    expect_all("alice fell hole", ["Learned"]),
    expect("who fell?", "alice fell"),
    % Irregular por distancia al unico verbo de esa entidad (ate~eat).
    expect_all("ana eat cake", ["Learned"]),
    expect("who ate cake?", "eats cake"),
    % C1: ensenar cierra el hueco abierto arriba y responde con prueba.
    expect_all("Ana abrio ventana", ["Learned", "GAP resolved"]),
    findall(G2, gap(G2, _, open, _, _), Open2),
    Open2 == [],
    expect("abrio ventana?", "ana abrio ventana"),
    % did + glue: si el mapa tiene el SVO, no es No por no parsear.
    expect_all("baby turned pig", ["Learned"]),
    expect("did the baby turn into a pig?", "Yes"),
    % Un ente, sin relacion viva: preguntar por el ente es listar sus hechos.
    expect("what did ana do?", "opened door"),
    % Verbo ES desconocido + ente: no volcar la vida del ente.
    capture("que comio ana?", OutComio),
    % "que paso con X": prep corta + verbo novel + ente = about, no dump-comio.
    expect("que paso con ana?", "opened"),
    % cliticos/preps de 1-2 letras no impiden el SVO (se/en/el).
    expect_all("el gato se comio el pan", ["Learned", "gato"]),
    expect("comio pan?", "gato"),
    ( sub_string(OutComio, _, _, _, "opened door") ->
        format('FAIL que-comio dumped: ~w~n', [OutComio]), fail
    ; true
    ),
    % Verbo desconocido (no qlead): no volcar todos los hechos del ente.
    capture("where did ana go?", OutGo),
    ( sub_string(OutGo, _, _, _, "opened door") ->
        format('FAIL where-go dumped facts: ~w~n', [OutGo]), fail
    ; true
    ),
    % "what about X" con X vivo: los hechos de X, sin lexico extra.
    expect("what about leo?", "leo"),
    expect_all("ana meet leo", ["Learned"]),
    expect_all("ana meet mia", ["Learned"]),
    expect("how many people did ana meet?", "2"),
    % "is there X" con X vivo: glue cae, quedan los hechos de X.
    expect("is there ana?", "opened"),
    % qnorm no reimprime assuming al backtrackear (bucle vistos en vivo).
    expect_all("cheshire left grin", ["Learned"]),
    capture("did the cheshire leave a grin?", OutLeave),
    findall(1, sub_string(OutLeave, _, _, _, "assuming"), As),
    length(As, NA),
    ( NA =< 1 -> true
    ; format('FAIL assuming flood ~w in ~w~n', [NA, OutLeave]), fail
    ),
    ( sub_string(OutLeave, _, _, _, "left") -> true
    ; format('FAIL leave~left ~w~n', [OutLeave]), fail
    ),
    % 3 tokens: el medio es la relacion, aunque el objeto ya sea vivo.
    expect_all("pastel means cake", ["Learned", "means"]),
    expect_all("comio means eat", ["Learned"]),
    % Pregunta corta sin anclas: explica el ultimo hecho (no es lexico).
    expect("who opened door?", "ana"),
    expect("por que?", "opened"),
    % Queja/dialogo con token corto y nada vivo: no es un hecho.
    capture("no me entero", OutNo),
    \+ sub_string(OutNo, _, _, _, "Learned"),
    \+ memory_relation(no, me, entero, _, _),
    % "que X?" sin X en el mapa: el libro, no un hueco muerto.
    expect("que libro?", "hechos"),
    expect("de que trata?", "alice"),
    % why-did + verbo del mapa (found) bajo forma de pregunta (find):
    % prueba, no Yes/No. Distancia al unico predicado, sin lista ate/eat.
    expect_all("ana found llave", ["Learned"]),
    capture("why did ana find the llave?", OutWhy),
    \+ sub_string(OutWhy, _, _, _, "Yes."),
    ( sub_string(OutWhy, _, _, _, "via") -> true
    ; format('FAIL why-did expected via, got ~w~n', [OutWhy]), fail
    ),
    % And X? continua LastQ. Si el mapa no cubre el hueco: unknown,
    % no pivota a about (cake es objeto de eat, no de opened).
    capture("who opened door?", _),
    capture("And the cake?", OutAndCake),
    \+ sub_string(OutAndCake, _, _, _, "eat"),
    \+ sub_string(OutAndCake, _, _, _, "almost nothing"),
    % what about X con hechos salientes de X: about, no elipsis de LastQ.
    expect_all("leo lives madrid", ["Learned"]),
    capture("who did ana meet?", _),
    expect("what about leo?", "lives"),
    % more continua los hechos mostrados (mismo verbo), no un nombre
    % suelto del sort de Xs.
    expect_all("ana meet kim", ["Learned"]),
    expect_all("ana meet rio", ["Learned"]),
    expect_all("ana meet sam", ["Learned"]),
    expect_all("ana meet pat", ["Learned"]),
    capture("who did ana meet?", OutMeet),
    ( sub_string(OutMeet, _, _, _, "more") ->
        capture("more", OutMore),
        ( sub_string(OutMore, _, _, _, "meet") -> true
        ; format('FAIL more should continue facts, got ~w~n', [OutMore]), fail
        )
    ; format('FAIL expected a capped meet list, got ~w~n', [OutMeet]), fail
    ),
    % what about X con hechos salientes: about, no elipsis del LastQ.
    expect_all("queen threatened ana", ["Learned"]),
    expect("what about queen?", "threatened"),
    expect("did she threaten ana?", "Yes"),
    % C3: phrase/2 ida y vuelta, terminales = simbolos vivos, sin lexico.
    dcg_roundtrip((ana, opened, door)),
    dcg_roundtrip((ana, abrio, puerta)),
    dcg_roundtrip((alice, followed, white_rabbit)),
    % what happened to X: "to" cae (L=2); el verbo novel ancla el ente.
    expect("what happened to the baby?", "turned"),
    % and parte dos SVO (sujeto compartido); ate~eat no se duplica.
    expect_all("nilo opened puerta and ate pan", ["Learned", "opened", "ate"]),
    expect("opened puerta?", "nilo opened puerta"),
    expect("ate pan?", "nilo"),
    % she + verbo unico del grafo: no pregunta Do you mean.
    expect_all("ria paints muro", ["Learned"]),
    expect_all("gil cooks sopa", ["Learned"]),
    capture("who paints muro?", _),
    capture("who cooks sopa?", _),
    capture("did she paint muro?", OutShe),
    \+ sub_string(OutShe, _, _, _, "Do you mean"),
    ( sub_string(OutShe, _, _, _, "Yes") -> true
    ; format('FAIL unique-she paint: ~w~n', [OutShe]), fail
    ),
    % D3 pending no se traga una pregunta nueva (quien/who).
    capture("did she win?", OutWin),
    ( sub_string(OutWin, _, _, _, "Do you mean") -> true
    ; format('FAIL expected clarify, got ~w~n', [OutWin]), fail
    ),
    capture("quien es ana?", OutQuien),
    \+ sub_string(OutQuien, _, _, _, "Do you mean"),
    ( sub_string(OutQuien, _, _, _, "opened") -> true
    ; sub_string(OutQuien, _, _, _, "ana") -> true
    ; format('FAIL quien-es swallowed: ~w~n', [OutQuien]), fail
    ),
    % and + objeto: comparte el verbo. "the" no se guarda como rel.
    expect_all("nilo met rio and the sam", ["Learned"]),
    expect("met rio?", "nilo"),
    expect("met sam?", "nilo"),
    \+ memory_relation(_, the, _, _, _),
    \+ memory_relation(nilo, the, _, _, _),
    % verbo novel sin prep corta: no volcar todos los hechos del ente.
    capture("who loves ana?", OutLoves),
    \+ sub_string(OutLoves, _, _, _, "opened door"),
    % into entre verbo y objeto cae; el sujeto no es "the".
    expect_all("kiko turned frog", ["Learned"]),
    capture("the kiko turned into a frog", OutInto),
    \+ sub_string(OutInto, _, _, _, "the --"),
    \+ memory_relation(the, _, _, _, _),
    expect("turned frog?", "kiko"),
    % love hereda hugs (stem) via means, sin lista love/hugs.
    expect_all("hugs means opened", ["Learned"]),
    expect("did ana hug door?", "Yes"),
    % verbo novel lejos en longitud: no se pega a otro del ente.
    capture("did ana vanish door?", OutVan),
    \+ sub_string(OutVan, _, _, _, "Yes"),
    capture("ok", OutOk),
    \+ sub_string(OutOk, _, _, _, "Learned"),
    \+ sub_string(OutOk, _, _, _, "I don't know").

dcg_roundtrip(T) :-
    ( surface_sent(T, Line),
      parse_svo_line(Line, T2),
      T == T2 -> true
    ; format('FAIL DCG roundtrip ~q~n', [T]),
      ( catch(surface_sent(T, L), E, L = error(E)) ->
            format('  surface: ~q~n', [L])
        ; format('  surface failed~n', [])
      ),
      fail
    ).

capture(Line, Out) :-
    with_output_to(string(Out), chat_line(Line)).

expect(Line, Sub) :-
    capture(Line, Out),
    ( sub_string(Out, _, _, _, Sub) -> true
    ; format('FAIL expect ~q in answer to ~q~n  got: ~w~n',
             [Sub, Line, Out]),
      fail
    ).

expect_all(Line, Subs) :-
    capture(Line, Out),
    ( forall(member(Sub, Subs), sub_string(Out, _, _, _, Sub)) -> true
    ; format('FAIL expect ~q in answer to ~q~n  got: ~w~n',
             [Subs, Line, Out]),
      fail
    ).
