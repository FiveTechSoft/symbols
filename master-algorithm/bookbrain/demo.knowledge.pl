% demo.knowledge.pl — KB curada para el demo del chat (P1).
% 20 hechos lexicos verificados a mano. Verbos elegidos compatibles con el
% stemmer del chat (bb_stem/2): la forma de pregunta y la guardada comparten
% stem o igualdad (open/opened, pour/poured, own/owns, cut/cut, read/read).
% Refs curated_N (provenance manual). Pasa puerta P1: set de 16 preguntas.
memfact(ana,opened,door,1.0,1).
memfact(leo,opened,window,1.0,1).
memfact(mia,poured,tea,1.0,1).
memfact(ana,poured,wine,1.0,1).
memfact(leo,visited,oslo,1.0,1).
memfact(ana,visited,paris,1.0,1).
memfact(mia,visited,madrid,1.0,1).
memfact(ana,owns,book,1.0,1).
memfact(leo,owns,compass,1.0,1).
memfact(mia,cut,bread,1.0,1).
memfact(leo,fixed,clock,1.0,1).
memfact(ana,signed,letter,1.0,1).
memfact(mia,read,book,1.0,1).
memfact(leo,closed,door,1.0,1).
memfact(ana,locked,chest,1.0,1).
memfact(mia,picked,apple,1.0,1).
memfact(leo,polished,bell,1.0,1).
memfact(ana,cleaned,floor,1.0,1).
memfact(mia,brushed,cat,1.0,1).
memfact(leo,trimmed,lamp,1.0,1).
provfact(ana,opened,door,curated_1,none,active).
provfact(leo,opened,window,curated_2,none,active).
provfact(mia,poured,tea,curated_3,none,active).
provfact(ana,poured,wine,curated_4,none,active).
provfact(leo,visited,oslo,curated_5,none,active).
provfact(ana,visited,paris,curated_6,none,active).
provfact(mia,visited,madrid,curated_7,none,active).
provfact(ana,owns,book,curated_8,none,active).
provfact(leo,owns,compass,curated_9,none,active).
provfact(mia,cut,bread,curated_10,none,active).
provfact(leo,fixed,clock,curated_11,none,active).
provfact(ana,signed,letter,curated_12,none,active).
provfact(mia,read,book,curated_13,none,active).
provfact(leo,closed,door,curated_14,none,active).
provfact(ana,locked,chest,curated_15,none,active).
provfact(mia,picked,apple,curated_16,none,active).
provfact(leo,polished,bell,curated_17,none,active).
provfact(ana,cleaned,floor,curated_18,none,active).
provfact(mia,brushed,cat,curated_19,none,active).
provfact(leo,trimmed,lamp,curated_20,none,active).
