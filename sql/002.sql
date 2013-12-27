ALTER TABLE version ADD COLUMN status char default 'p';
UPDATE version SET status = 'a' where approved = true;
ALTER TABLE version DROP column approved;
