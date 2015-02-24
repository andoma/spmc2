ALTER TABLE plugin ADD COLUMN popularity FLOAT;

CREATE INDEX plugin_popularity ON plugin (popularity);
