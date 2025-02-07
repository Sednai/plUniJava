-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION plunijava" to load this file. \quit

CREATE FUNCTION java_call_handler() RETURNS language_handler
    AS 'MODULE_PATHNAME'
    LANGUAGE C;

CREATE LANGUAGE UJAVA
    HANDLER java_call_handler;

CREATE FUNCTION pluj_kill_global_workers() RETURNS INT
    AS 'MODULE_PATHNAME'
    LANGUAGE C;

CREATE FUNCTION pluj_kill_user_workers() RETURNS INT
    AS 'MODULE_PATHNAME'
    LANGUAGE C;

CREATE FUNCTION pluj_show_user_queue() RETURNS INT
    AS 'MODULE_PATHNAME'
    LANGUAGE C;

CREATE FUNCTION pluj_clear_user_queue() RETURNS INT
    AS 'MODULE_PATHNAME'
    LANGUAGE C;


