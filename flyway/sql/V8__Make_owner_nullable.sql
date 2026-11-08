-- Make owner_id nullable to support nested flows (child flows don't have owner)
ALTER TABLE flows
ALTER COLUMN owner_id DROP NOT NULL;

-- Update foreign key constraint to allow NULL
ALTER TABLE flows
DROP CONSTRAINT fk_flows_owner;

ALTER TABLE flows
ADD CONSTRAINT fk_flows_owner
FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE;
